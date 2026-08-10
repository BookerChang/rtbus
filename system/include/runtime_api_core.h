/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RUNTIME_API_CORE_H_
#define RUNTIME_API_CORE_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_WISNODEZ_API_TABLE_BASE)
#define WZ_API_TABLE_BASE CONFIG_WISNODEZ_API_TABLE_BASE
#else
extern const uint8_t __wisnodez_api_table_base[];
#define WZ_API_TABLE_BASE ((uintptr_t)__wisnodez_api_table_base)
#endif
#define WZ_API_MAGIC      0x575A4150U
#define WZ_API_VERSION    6U
#define WZ_ENOSYS         38
#define WZ_EINVAL         22
#define WZ_EMSGSIZE       90
#define WZ_EINPROGRESS    115
#define WZ_WAIT_FOREVER   UINT32_MAX
#define RUNTIME_EVENT_VALUE_MAX 256U

#include "runtime_abi.h"

enum wisnodez_api_slot {
    WISNODEZ_API_SLOT_MAGIC = 0,
    WISNODEZ_API_SLOT_VERSION,
    WISNODEZ_API_SLOT_SLOT_COUNT,
    WISNODEZ_API_SLOT_SCHEDULE_POST,
    WISNODEZ_API_SLOT_DELAY,
    WISNODEZ_API_SLOT_PRINTK,
    WISNODEZ_API_SLOT_VPRINTK,
    WISNODEZ_API_SLOT_RESULT_WAIT,
    WISNODEZ_API_SLOT_SERIAL_WRITE,
    WISNODEZ_API_SLOT_GPIO_CONFIGURE,
    WISNODEZ_API_SLOT_GPIO_WRITE,
    WISNODEZ_API_SLOT_GPIO_READ,
    WISNODEZ_API_SLOT_ON_EVENT,
    WISNODEZ_API_SLOT_COUNT,
};

struct runtime_event_tlv {
    uint16_t group;
    uint16_t tag;
    uint16_t len;
    uint16_t flags;
    uint8_t value[RUNTIME_EVENT_VALUE_MAX];
};

typedef int (*runtime_schedule_post_api_t)(uint8_t task_id, uint32_t ctx_id,
                                           const void *payload,
                                           uint32_t payload_len);
typedef int32_t (*runtime_delay_api_t)(uint32_t delay_ms);
typedef void (*runtime_printk_api_t)(const char *fmt, ...);
typedef void (*runtime_vprintk_api_t)(const char *fmt, va_list args);
typedef int (*runtime_result_wait_api_t)(uint32_t timeout_ms);
typedef size_t (*runtime_serial_write_api_t)(const uint8_t *data, size_t size);
typedef int (*runtime_gpio_configure_api_t)(uint32_t pin, uint32_t mode);
typedef int (*runtime_gpio_write_api_t)(uint32_t pin, uint32_t value);
typedef int (*runtime_gpio_read_api_t)(uint32_t pin);
typedef void (*runtime_event_callback_t)(
    const struct runtime_event_tlv *event);
typedef int (*runtime_on_event_api_t)(runtime_event_callback_t callback);

#define WZ_API_SLOT_PTR(slot) \
    (((volatile uintptr_t *)(uintptr_t)WZ_API_TABLE_BASE)[slot])
#define WZ_API_FN(slot, type) \
    ((type)WZ_API_SLOT_PTR(slot))

static inline int runtime_api_is_ready(void)
{
    return WZ_API_SLOT_PTR(WISNODEZ_API_SLOT_MAGIC) == WZ_API_MAGIC &&
           WZ_API_SLOT_PTR(WISNODEZ_API_SLOT_VERSION) == WZ_API_VERSION &&
           WZ_API_SLOT_PTR(WISNODEZ_API_SLOT_SLOT_COUNT) >= WISNODEZ_API_SLOT_COUNT;
}

static inline int runtime_on_event(runtime_event_callback_t callback)
{
    runtime_on_event_api_t on_event;

    on_event = WZ_API_FN(WISNODEZ_API_SLOT_ON_EVENT,
                         runtime_on_event_api_t);
    if (on_event == 0) {
        return -WZ_ENOSYS;
    }

    return on_event(callback);
}

static inline int runtime_post_wait_result(uint8_t task_id, uint32_t ctx_id,
                                           const void *payload,
                                           uint32_t payload_len,
                                           volatile int32_t *result)
{
    runtime_schedule_post_api_t post;
    runtime_result_wait_api_t wait_result;
    int ret;

    *result = -WZ_EINPROGRESS;
    post = WZ_API_FN(WISNODEZ_API_SLOT_SCHEDULE_POST,
                     runtime_schedule_post_api_t);
    wait_result = WZ_API_FN(WISNODEZ_API_SLOT_RESULT_WAIT,
                            runtime_result_wait_api_t);

    ret = post(task_id, ctx_id, payload, payload_len);
    if (ret != 0) {
        return ret;
    }

    ret = wait_result(WZ_WAIT_FOREVER);
    if (ret != 0) {
        return ret;
    }

    return *result;
}

#if defined(WISNODEZ_APPLICATION_SHORT_NAMES) && !defined(WZ_API_NO_SHORT_NAMES)
#define schedule_post       WZ_API_FN(WISNODEZ_API_SLOT_SCHEDULE_POST, runtime_schedule_post_api_t)
#define k_delay             WZ_API_FN(WISNODEZ_API_SLOT_DELAY, runtime_delay_api_t)
#define printk              WZ_API_FN(WISNODEZ_API_SLOT_PRINTK, runtime_printk_api_t)
#define vprintk             WZ_API_FN(WISNODEZ_API_SLOT_VPRINTK, runtime_vprintk_api_t)
#endif

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_API_CORE_H_ */
