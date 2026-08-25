/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/logging/log.h>

#include <rtbus.h>
#include <rtbus/native_service.h>

#include "runtime_abi.h"

LOG_MODULE_REGISTER(rtbus_bootstrap, LOG_LEVEL_INF);

static uint8_t rtbus_bootstrap_process(rtbus_ctx_t *ctx)
{
    int ret;

    switch (ctx->id) {
    case RTBUS_SYS_EVT(RTBUS_SYS_INIT):
        ret = native_service_start();
        if (ret != 0) {
            LOG_ERR("Native service start failed: %d", ret);
        }
        break;

    default:
        break;
    }

    return 0;
}

RTBUS_TASK_REGISTER(rtbus_bootstrap_task,
                    APPLICATION_TASK_NATIVE_BOOTSTRAP,
                    RTBUS_TASK_PRIORITY_LOW,
                    rtbus_bootstrap_process);
