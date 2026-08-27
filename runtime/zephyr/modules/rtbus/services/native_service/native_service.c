/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <rtbus/native_service.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/devicetree/fixed-partitions.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <rtbus_image.h>
#include <rtbus.h>
#include <rtbus/runtime_event.h>

#include "runtime_api.h"

#ifndef __strong
#define __strong
#endif

#if !defined(CONFIG_RTBUS_NATIVE_API_TABLE_BASE)
#error "CONFIG_RTBUS_NATIVE_API_TABLE_BASE is required by native_service"
#endif

#define NATIVE_RAM_NODE            DT_NODELABEL(shared_upgrade_ram)
#define NATIVE_RAM_BASE            DT_REG_ADDR(NATIVE_RAM_NODE)
#define NATIVE_RAM_POOL_SIZE       DT_REG_SIZE(NATIVE_RAM_NODE)
#define NATIVE_RAM_SIZE            CONFIG_RTBUS_IMAGE_RAM_MAX
#define NATIVE_API_NODE            DT_NODELABEL(shared_ram)
#define NATIVE_API_BASE            DT_REG_ADDR(NATIVE_API_NODE)
#define NATIVE_API_SIZE            DT_REG_SIZE(NATIVE_API_NODE)
#define NATIVE_FLASH_BASE          DT_FIXED_PARTITION_ADDR(DT_NODELABEL(component_slot_partition))
#define NATIVE_RTBUS_POST_QUEUE_DEPTH   8
#define NATIVE_RTBUS_POST_PAYLOAD_MAX   32
#define NATIVE_EVENT_QUEUE_DEPTH           8
#define NATIVE_UPGRADE_HANDOFF_TIMEOUT_MS 2000
#define NATIVE_REQUIRED_ABI_VERSION        RTBUS_API_VERSION
#define NATIVE_WAKE_IRQ       (1U << 0)
#define NATIVE_WAKE_PROCESS   (1U << 1)
#define NATIVE_WAKE_ALL       (NATIVE_WAKE_IRQ | NATIVE_WAKE_PROCESS)

BUILD_ASSERT(CONFIG_RTBUS_IMAGE_RAM_MAX <= NATIVE_RAM_POOL_SIZE,
             "CONFIG_RTBUS_IMAGE_RAM_MAX exceeds shared_upgrade_ram");
BUILD_ASSERT(CONFIG_RTBUS_NATIVE_API_TABLE_BASE == NATIVE_API_BASE,
             "CONFIG_RTBUS_NATIVE_API_TABLE_BASE must match shared_ram");
BUILD_ASSERT(RTBUS_API_SLOT_COUNT * sizeof(uintptr_t) <= NATIVE_API_SIZE,
             "shared_ram is too small for RTBus API table");
BUILD_ASSERT(APPLICATION_LORAWAN_SEND_HEADER_SIZE +
             APPLICATION_LORAWAN_SEND_PAYLOAD_MAX <=
             NATIVE_RTBUS_POST_PAYLOAD_MAX,
             "Application LoRaWAN send payload exceeds rtbus post budget");

typedef int (*native_entry_t)(void);

K_THREAD_STACK_DEFINE(native_thread_stack, 1024);

static struct k_thread native_thread;
static k_tid_t native_thread_id;
static native_entry_t native_entry;
/*
 * shared_upgrade_ram is reused for two different lifetimes:
 * - normal runtime: native application RAM (.data/.bss/heap-like workspace)
 * - upgrade: board-dependent staging for RAM-backed PATCH/APP DFU paths
 *
 * Direct-flash DFU boards do not use this RAM for the transfer payload, but
 * callers still stop/park the native application before replacing the component
 * slot. LoRaWAN FUOTA does that when business uplink is blocked (~20 s before
 * Class C). YModem/APP paths still call native_service_stop_for_upgrade()
 * explicitly. PATCH store begin no longer stops the app by itself.
 */
static atomic_t native_patch_handoff;
static atomic_t native_last_error;
static atomic_t native_console_suppressed;
/*
 * This is the handoff-side "is rtbus post busy?" guard.
 *
 * Schedule does not expose a lock state to native_service, so the runtime
 * marks the two rtbus-post related critical sections itself:
 * - while copying an application payload into native_rtbus_post_msgq
 * - while the runtime drains that queue and calls the rtbus post API
 *
 * A zero value means the native application thread can be force-aborted for an
 * upgrade handoff. Queued rtbus messages and result waits are runtime-owned
 * and can be purged without asking the application.
 */
static atomic_t native_api_busy;

struct native_rtbus_post_msg {
    uint8_t task_id;
    uint8_t reserved[3];
    uint32_t ctx_id;
    uint32_t payload_len;
    uint8_t payload[NATIVE_RTBUS_POST_PAYLOAD_MAX];
};

K_MSGQ_DEFINE(native_rtbus_post_msgq,
             sizeof(struct native_rtbus_post_msg),
             NATIVE_RTBUS_POST_QUEUE_DEPTH,
             4);

K_MSGQ_DEFINE(native_event_msgq,
             sizeof(struct runtime_event_tlv),
             NATIVE_EVENT_QUEUE_DEPTH,
             4);

K_EVENT_DEFINE(native_wake_event);

/* Completion signal only; the actual return value is written to result_addr. */
K_SEM_DEFINE(native_result_sem, 0, 1);

static runtime_event_callback_t native_event_callback;

int native_service_emit_event(uint16_t group, uint16_t tag,
                                   const void *value, uint16_t len)
{
    struct runtime_event_tlv event = {
        .group = group,
        .tag = tag,
        .len = len,
    };
    int ret;

    if (len > RUNTIME_EVENT_VALUE_MAX) {
        return -EMSGSIZE;
    }

    if (len != 0U && value == NULL) {
        return -EINVAL;
    }

    if (len != 0U) {
        memcpy(event.value, value, len);
    }

    ret = k_msgq_put(&native_event_msgq, &event, K_NO_WAIT);
    if (ret == 0) {
        (void)k_event_post(&native_wake_event, NATIVE_WAKE_IRQ);
    }

    return ret;
}

static void native_service_dispatch_events(void)
{
    struct runtime_event_tlv event;

    while (k_msgq_get(&native_event_msgq, &event, K_NO_WAIT) == 0) {
        if (native_event_callback != NULL) {
            native_event_callback(&event);
        }
    }
}

/*
 * Private fragments for this service only.
 *
 * These files are included directly so API/ABI sections can share the service
 * state above without creating private headers or extra compilation units.
 */
#include "api.c"
#include "abi.c"

static bool native_result_addr_valid(uint32_t result_addr)
{
    uintptr_t addr = (uintptr_t)result_addr;

    return (addr % sizeof(int32_t)) == 0U &&
           addr >= NATIVE_RAM_BASE &&
           addr <= (NATIVE_RAM_BASE + NATIVE_RAM_SIZE - sizeof(int32_t));
}

int native_service_last_error(void)
{
    return (int)atomic_get(&native_last_error);
}

void native_service_suppress_console(bool suppress)
{
    atomic_set(&native_console_suppressed, suppress ? 1 : 0);
}

int native_service_complete_result(uint32_t result_addr, int32_t result)
{
    volatile int32_t *target;

    if (result_addr == 0U) {
        return 0;
    }

    if (!native_result_addr_valid(result_addr)) {
        return -EINVAL;
    }

    target = (volatile int32_t *)(uintptr_t)result_addr;
    *target = result;
    k_sem_give(&native_result_sem);
    (void)k_event_post(&native_wake_event, NATIVE_WAKE_PROCESS);

    return 0;
}

void native_service_request_stop_for_upgrade(void)
{
    /*
     * This is a request only. The native thread will park cooperatively if it
     * reaches the thread loop, while native_service_handoff_ram_for_patch()
     * may force-abort it once no runtime API critical section is active.
     */
    atomic_set(&native_patch_handoff, 1);
    native_service_suppress_console(true);
    (void)k_event_post(&native_wake_event, NATIVE_WAKE_PROCESS);

    if (native_thread_id == NULL) {
        return;
    }

    k_wakeup(native_thread_id);
}

static void native_service_force_stop_thread(void)
{
    /*
     * Force stop is intentionally limited to the native thread. Runtime-owned
     * queues/semaphores are reset here so APP and PATCH upgrades can take over
     * shared_upgrade_ram immediately after the handoff succeeds.
     */
    if (native_thread_id != NULL) {
        k_thread_abort(native_thread_id);
        native_thread_id = NULL;
    }

    native_entry = NULL;
    native_event_callback = NULL;
    k_msgq_purge(&native_rtbus_post_msgq);
    k_msgq_purge(&native_event_msgq);
    k_sem_reset(&native_result_sem);
    (void)k_event_clear(&native_wake_event, NATIVE_WAKE_ALL);
}

int native_service_handoff_ram_for_patch(void)
{
    int64_t deadline = k_uptime_get() + NATIVE_UPGRADE_HANDOFF_TIMEOUT_MS;

    native_service_request_stop_for_upgrade();

    if (native_thread_id == NULL) {
        return 0;
    }

    while (true) {
        /*
         * This is the stop-before-upgrade gate: do not abort while an
         * application-originated rtbus post is inside the runtime-owned
         * critical section. Otherwise, force stop is allowed even if the
         * application is sleeping or running unrelated code.
         */
        if (atomic_get(&native_api_busy) == 0) {
            native_service_force_stop_thread();
            return 0;
        }

        if (k_uptime_get() >= deadline) {
            return -ETIMEDOUT;
        }

        k_sleep(K_MSEC(10));
    }

    return 0;
}

int native_service_stop_for_upgrade(void)
{
    return native_service_handoff_ram_for_patch();
}

void native_service_resume_after_upgrade(void)
{
    /*
     * After an APP upgrade the image in flash changed. Return to the no-thread
     * state, then start only if the new component validates cleanly.
     */
    atomic_set(&native_patch_handoff, 0);
    atomic_set(&native_api_busy, 0);
    native_service_force_stop_thread();
    (void)native_service_start();
    native_service_suppress_console(false);
}


static int native_validate_metadata(const struct rtbus_image_status *status,
                                                const struct rtbus_image_native_metadata *metadata)
{
    uint32_t abi_version;

    if ((status->flags & RTBUS_IMAGE_FLAG_NATIVE) == 0U) {
        return -ENOTSUP;
    }

    abi_version = RTBUS_IMAGE_FLAGS_ABI_VERSION(status->flags);
    if (abi_version != NATIVE_REQUIRED_ABI_VERSION) {
        printk("application: ERROR incompatible ABI version %u, runtime requires %u\n",
               (unsigned int)abi_version,
               (unsigned int)NATIVE_REQUIRED_ABI_VERSION);
        return -EPROTONOSUPPORT;
    }

    if (metadata->entry_offset >= status->payload_size ||
        metadata->data_load_offset > status->payload_size ||
        metadata->data_size > status->payload_size - metadata->data_load_offset ||
        metadata->data_ram_offset > NATIVE_RAM_SIZE ||
        metadata->data_size > NATIVE_RAM_SIZE - metadata->data_ram_offset ||
        metadata->bss_ram_offset > NATIVE_RAM_SIZE ||
        metadata->bss_size > NATIVE_RAM_SIZE - metadata->bss_ram_offset) {
        return -EINVAL;
    }

    return 0;
}

static int native_load(void)
{
    struct rtbus_image_status status;
    struct rtbus_image_native_metadata metadata;
    uintptr_t entry_addr;
    size_t read_size = 0U;
    uint8_t *native_ram = (uint8_t *)(uintptr_t)NATIVE_RAM_BASE;
    int rc;

    rc = rtbus_image_get_status(&status);
    if (rc != 0) {
        atomic_set(&native_last_error, rc);
        return rc;
    }

    rc = rtbus_image_get_native_metadata(&metadata);
    if (rc != 0) {
        atomic_set(&native_last_error, rc);
        return rc;
    }

    rc = native_validate_metadata(&status, &metadata);
    if (rc != 0) {
        atomic_set(&native_last_error, rc);
        return rc;
    }

    rc = rtbus_image_read_payload(metadata.data_load_offset,
                                   native_ram + metadata.data_ram_offset,
                                   metadata.data_size,
                                   &read_size);
    if (rc != 0) {
        atomic_set(&native_last_error, rc);
        return rc;
    }
    if (read_size != metadata.data_size) {
        atomic_set(&native_last_error, -EIO);
        return -EIO;
    }

    memset(native_ram + metadata.bss_ram_offset, 0, metadata.bss_size);

    entry_addr = (uintptr_t)NATIVE_FLASH_BASE + RTBUS_IMAGE_HEADER_SIZE +
                 metadata.entry_offset;
    native_entry = (native_entry_t)(entry_addr | 1U);
    atomic_set(&native_last_error, 0);
    printk("application: loaded entry=0x%lx data=%u bss=%u ram=0x%lx/%u\n",
           (unsigned long)(entry_addr | 1U),
           (unsigned int)metadata.data_size,
           (unsigned int)metadata.bss_size,
           (unsigned long)(uintptr_t)NATIVE_RAM_BASE,
           (unsigned int)MIN((size_t)status.required_ram_size,
                             (size_t)NATIVE_RAM_SIZE));

    return 0;
}

static void native_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        if (atomic_get(&native_patch_handoff) != 0) {
            k_sleep(K_FOREVER);
            continue;
        }

        native_service_dispatch_events();
        /* Native application execution point. */
        (void)native_entry();
        native_service_dispatch_events();
    }
}

int native_service_start(void)
{
    int rc;

    if (atomic_get(&native_patch_handoff) != 0) {
        return -EBUSY;
    }

    if (native_thread_id != NULL) {
        return 0;
    }

    native_api_table_install();

    rc = native_load();
    if (rc != 0) {
        native_service_force_stop_thread();
        atomic_set(&native_last_error, rc);
        if (rc == -ENODATA) {
            printk("application: ERROR no valid native image in component slot\n");
        } else {
            printk("application: ERROR image rejected (%d)\n", rc);
        }
        return rc;
    }

    /*
     * The native application owns a Zephyr thread, not an EMOS task. It can
     * block independently and only submits requests back through the API table.
     */
    native_thread_id = k_thread_create(&native_thread,
                                            native_thread_stack,
                                            K_THREAD_STACK_SIZEOF(native_thread_stack),
                                            native_thread_entry,
                                            NULL, NULL, NULL,
                                            K_PRIO_PREEMPT(10),
                                            0,
                                            K_NO_WAIT);
    k_thread_name_set(native_thread_id, "native");

    return 0;
}
