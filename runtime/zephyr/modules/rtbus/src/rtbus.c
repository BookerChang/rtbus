/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define RTBUS_NO_INIT_MACRO
#define EMOS_INTERNAL_BUILD
#include "rtbus.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/*
 * EMOS is compiled into this facade as a private core implementation.
 * This keeps raw emos_* entry points static while mod_rtbus exposes the
 * public rtbus_* API, hooks, and delayed-event behavior.
 */
#define EMOS_CORE_API static
#define EMOS_POST_AFTER rtbus_post_after
#define EMOS_PROCESS_ASYNC_POSTS rtbus_process_async_posts
#include "emos.c"
#undef entry_count
#undef task_tbl
#undef poll_next_index

#ifndef CONFIG_RTBUS_DELAYED_EVENT_DEPTH
#define CONFIG_RTBUS_DELAYED_EVENT_DEPTH 8
#endif

#ifndef CONFIG_RTBUS_DELAYED_EVENT_PAYLOAD_MAX
#define CONFIG_RTBUS_DELAYED_EVENT_PAYLOAD_MAX 32
#endif

struct rtbus_delayed_event {
    bool used;
    uint8_t task_id;
    uint32_t ctx_id;
    uint32_t payload_len;
    uint32_t due_ms;
    uint8_t payload[CONFIG_RTBUS_DELAYED_EVENT_PAYLOAD_MAX];
};

static struct rtbus_delayed_event
    delayed_events[CONFIG_RTBUS_DELAYED_EVENT_DEPTH];

static bool rtbus_due_reached(uint32_t now_ms, uint32_t due_ms)
{
    return (int32_t)(now_ms - due_ms) >= 0;
}

static int32_t rtbus_timeout_until(uint32_t now_ms, uint32_t due_ms)
{
    int32_t timeout_ms = (int32_t)(due_ms - now_ms);

    return timeout_ms < 0 ? 0 : timeout_ms;
}

static void rtbus_reset_delayed_events(void)
{
    memset(delayed_events, 0, sizeof(delayed_events));
}

int rtbus_init(uint8_t *pool_addr, uint16_t pool_size,
                  const emos_entry_t *entry_tbl, uint8_t entry_count)
{
    int rc;

    rtbus_reset_delayed_events();

    rc = emos_init(pool_addr, pool_size, entry_tbl, entry_count);
    if (rc != 0) {
        rtbus_reset_delayed_events();
    }

    return rc;
}

int rtbus_post(uint8_t task_id, uint32_t ctx_id, const void *payload,
                  uint32_t payload_len)
{
    int rc;

    rc = emos_post(task_id, ctx_id, payload, payload_len);
    if (rc == -ENOENT) {
        return -ENOTSUP;
    }

    return rc;
}

__weak void rtbus_post_after(uint8_t task_id, uint32_t ctx_id,
                                const void *payload, uint32_t payload_len)
{
    ARG_UNUSED(task_id);
    ARG_UNUSED(ctx_id);
    ARG_UNUSED(payload);
    ARG_UNUSED(payload_len);
}

__weak int rtbus_process_async_posts(void)
{
    return 0;
}

int rtbus_post_delay(uint8_t task_id, uint32_t ctx_id, const void *payload,
                        uint32_t payload_len, uint32_t delay_ms)
{
    struct rtbus_delayed_event *event = NULL;

    if (delay_ms == 0U) {
        return rtbus_post(task_id, ctx_id, payload, payload_len);
    }

    if (payload_len != 0U && payload == NULL) {
        return -EINVAL;
    }

    if (payload_len > CONFIG_RTBUS_DELAYED_EVENT_PAYLOAD_MAX) {
        return -EMSGSIZE;
    }

    for (size_t i = 0; i < ARRAY_SIZE(delayed_events); i++) {
        if (!delayed_events[i].used) {
            event = &delayed_events[i];
            break;
        }
    }

    if (event == NULL) {
        return -ENOMEM;
    }

    event->used = true;
    event->task_id = task_id;
    event->ctx_id = ctx_id;
    event->payload_len = payload_len;
    event->due_ms = k_uptime_get_32() + delay_ms;

    if (payload_len != 0U) {
        memcpy(event->payload, payload, payload_len);
    }

    rtbus_post_after(task_id, ctx_id, payload, payload_len);

    return 0;
}

int32_t rtbus_next_timeout_ms(void)
{
    bool found = false;
    uint32_t now_ms = k_uptime_get_32();
    int32_t next_timeout_ms = 0;

    for (size_t i = 0; i < ARRAY_SIZE(delayed_events); i++) {
        int32_t timeout_ms;

        if (!delayed_events[i].used) {
            continue;
        }

        timeout_ms = rtbus_timeout_until(now_ms, delayed_events[i].due_ms);
        if (!found || timeout_ms < next_timeout_ms) {
            next_timeout_ms = timeout_ms;
            found = true;
        }
    }

    return found ? next_timeout_ms : -1;
}

static int rtbus_move_due_events(void)
{
    uint32_t now_ms = k_uptime_get_32();

    for (size_t i = 0; i < ARRAY_SIZE(delayed_events); i++) {
        struct rtbus_delayed_event *event = &delayed_events[i];
        int rc;

        if (!event->used || !rtbus_due_reached(now_ms, event->due_ms)) {
            continue;
        }

        rc = emos_post(event->task_id, event->ctx_id,
                       event->payload_len == 0U ? NULL : event->payload,
                       event->payload_len);
        if (rc != 0) {
            return rc;
        }

        memset(event, 0, sizeof(*event));
    }

    return 0;
}

int rtbus_process(void)
{
    int rc;

    rc = rtbus_move_due_events();
    if (rc != 0) {
        return rc;
    }

    return emos_process();
}
