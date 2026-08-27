/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_AT_CLI_API_H_
#define RUNTIME_AT_CLI_API_H_

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int runtime_cli_post(const char *line)
{
    struct application_at_cli_exec_line_req req;

    if (line == 0) {
        return -RTBUS_EINVAL;
    }

    req.address = (uint32_t)(uintptr_t)line;
    req.done_task_id = 0U;
    req.done_event_id = 0U;

    return rtbus_post(APPLICATION_TASK_AT_CLI,
                      APPLICATION_AT_CLI_EXEC_LINE,
                      &req,
                      sizeof(req));
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_AT_CLI_API_H_ */
