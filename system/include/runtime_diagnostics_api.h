/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_DIAGNOSTICS_API_H_
#define RUNTIME_DIAGNOSTICS_API_H_

#include <stdint.h>

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int runtime_diagnostics_add(int32_t lhs, int32_t rhs)
{
    struct application_diagnostics_add_req req;
    /* Keep the completion result in application RAM for runtime write-back. */
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.lhs = lhs;
    req.rhs = rhs;

    return rtbus_post_wait_result(APPLICATION_TASK_DIAGNOSTICS,
                                  APPLICATION_DIAGNOSTICS_ADD,
                                  &req,
                                  APPLICATION_DIAGNOSTICS_ADD_PAYLOAD_SIZE,
                                  &result);
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_DIAGNOSTICS_API_H_ */
