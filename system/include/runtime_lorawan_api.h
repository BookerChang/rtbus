/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RUNTIME_LORAWAN_API_H_
#define RUNTIME_LORAWAN_API_H_

#include <stdint.h>

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int runtime_lorawan_send(uint8_t port, const void *data,
                                       uint8_t len, uint8_t confirmed)
{
    struct application_lorawan_send_req req;
    const uint8_t *bytes = (const uint8_t *)data;
    /* Keep the completion result in application RAM for runtime write-back. */
    static volatile int32_t result;

    if (len > APPLICATION_LORAWAN_SEND_PAYLOAD_MAX) {
        return -WZ_EMSGSIZE;
    }

    if (len != 0U && data == 0) {
        return -WZ_EINVAL;
    }

    req.port = port;
    req.confirmed = confirmed != 0U ? 1U : 0U;
    req.len = len;
    req.reserved = 0U;
    req.result_addr = (uint32_t)(uintptr_t)&result;

    for (uint8_t i = 0; i < len; i++) {
        req.data[i] = bytes[i];
    }

    return runtime_post_wait_result(APPLICATION_TASK_LORAWAN_SERVICE,
                                    APPLICATION_LORAWAN_SEND,
                                    &req,
                                    APPLICATION_LORAWAN_SEND_HEADER_SIZE + len,
                                    &result);
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_LORAWAN_API_H_ */
