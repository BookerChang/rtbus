/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_VALIDATION_API_H_
#define RUNTIME_VALIDATION_API_H_

#include <stdint.h>

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int runtime_validation_add(int32_t lhs, int32_t rhs)
{
    struct application_validation_add_req req;
    /* Keep the completion result in application RAM for runtime write-back. */
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.lhs = lhs;
    req.rhs = rhs;

    return runtime_post_wait_result(APPLICATION_TASK_VALIDATION_SERVICE,
                                    APPLICATION_VALIDATION_ADD,
                                    &req,
                                    APPLICATION_VALIDATION_ADD_PAYLOAD_SIZE,
                                    &result);
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_VALIDATION_API_H_ */
