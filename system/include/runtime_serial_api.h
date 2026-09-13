/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_SERIAL_API_H_
#define RUNTIME_SERIAL_API_H_

#include <stdint.h>

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int runtime_serial_begin(uint32_t port, uint32_t baud)
{
    struct application_runtime_serial_begin_req req;
    /* Keep the completion result in application RAM for runtime write-back. */
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.port = port;
    req.baud = baud;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_SERIAL,
                                  APPLICATION_RUNTIME_SERIAL_BEGIN,
                                  &req,
                                  APPLICATION_RUNTIME_SERIAL_BEGIN_PAYLOAD_SIZE,
                                  &result);
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_SERIAL_API_H_ */
