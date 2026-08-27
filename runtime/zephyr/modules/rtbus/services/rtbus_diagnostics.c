/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>

#include <rtbus.h>
#include <rtbus/native_service.h>

#include "runtime_abi.h"

LOG_MODULE_REGISTER(rtbus_diagnostics, LOG_LEVEL_INF);

static uint8_t rtbus_diagnostics_process(rtbus_ctx_t *ctx)
{
    int ret;

    switch (ctx->id) {
        case RTBUS_SYS_EVT(RTBUS_SYS_INIT):
            ret = native_service_start();
            if (ret != 0) {
                LOG_ERR("Native service start failed: %d", ret);
            }
            break;

#if defined(CONFIG_RTBUS_DIAGNOSTICS_SERVICE)
        case APPLICATION_DIAGNOSTICS_ADD:
            {
                struct application_diagnostics_add_req req;
                int32_t result;

                if (ctx->payload_len != sizeof(req)) {
                    LOG_ERR("diagnostics add payload size mismatch: %u", ctx->payload_len);
                    return 0;
                }

                memcpy(&req, ctx->payload, sizeof(req));
                result = req.lhs + req.rhs;

                ret = native_service_complete_result(req.result_addr, result);
                if (ret != 0) {
                    LOG_ERR("diagnostics add result completion failed: %d", ret);
                    return 0;
                }

            }
            break;
#endif
        default:
            break;
    }

    return 0;
}

RTBUS_TASK_REGISTER(rtbus_diagnostics_task,
                    APPLICATION_TASK_DIAGNOSTICS,
                    RTBUS_TASK_PRIORITY_LOW,
                    rtbus_diagnostics_process);
