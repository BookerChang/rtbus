/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtbus/runtime_event.h>

#include <zephyr/kernel.h>

#include <rtbus.h>

#ifndef __strong
#define __strong
#endif

K_SEM_DEFINE(runtime_event_sem, 0, 1);

void runtime_event_signal(void)
{
    k_sem_give(&runtime_event_sem);
}

int runtime_event_wait(uint32_t timeout_ms)
{
    return k_sem_take(&runtime_event_sem, K_MSEC(timeout_ms));
}

__strong void rtbus_post_after(uint8_t task_id, uint32_t ctx_id,
                         const void *payload, uint32_t payload_len)
{
    ARG_UNUSED(task_id);
    ARG_UNUSED(ctx_id);
    ARG_UNUSED(payload);
    ARG_UNUSED(payload_len);

    runtime_event_signal();
}
