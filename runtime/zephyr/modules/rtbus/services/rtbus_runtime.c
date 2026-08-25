/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rtbus.h>
#include <rtbus/runtime_event.h>

LOG_MODULE_REGISTER(rtbus_runtime, LOG_LEVEL_INF);

#ifndef CONFIG_RTBUS_RUNTIME_THREAD_STACK_SIZE
#define CONFIG_RTBUS_RUNTIME_THREAD_STACK_SIZE 1024
#endif

#ifndef CONFIG_RTBUS_RUNTIME_THREAD_PRIORITY
#define CONFIG_RTBUS_RUNTIME_THREAD_PRIORITY 9
#endif

static uint8_t rtbus_runtime_pool[CONFIG_RUNTIME_POOL_SIZE] __aligned(4);

static void rtbus_runtime_thread_entry(void *p1, void *p2, void *p3)
{
    int ret;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    ret = rtbus_init(rtbus_runtime_pool, CONFIG_RUNTIME_POOL_SIZE, NULL, 0);
    if (ret != 0) {
        LOG_ERR("RTBus initialization failed: %d", ret);
        return;
    }

    while (true) {
        int32_t timeout_ms;

        ret = rtbus_process();
        if (ret != 0) {
            LOG_ERR("RTBus process failed: %d", ret);
        }

        timeout_ms = rtbus_next_timeout_ms();
        if (timeout_ms < 0 || timeout_ms > CONFIG_RTBUS_RUNTIME_TIMEOUT_MAX_MS) {
            timeout_ms = CONFIG_RTBUS_RUNTIME_TIMEOUT_MAX_MS;
        }

        (void)runtime_event_wait((uint32_t)timeout_ms);
    }
}

K_THREAD_DEFINE(rtbus_runtime_thread,
                CONFIG_RTBUS_RUNTIME_THREAD_STACK_SIZE,
                rtbus_runtime_thread_entry,
                NULL, NULL, NULL,
                K_PRIO_PREEMPT(CONFIG_RTBUS_RUNTIME_THREAD_PRIORITY),
                0,
                0);
