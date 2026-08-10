/*
 * SPDX-License-Identifier: Apache-2.0
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
    runtime_schedule_post_api_t post;

    if (line == 0) {
        return -WZ_EINVAL;
    }

    post = WZ_API_FN(WISNODEZ_API_SLOT_SCHEDULE_POST,
                     runtime_schedule_post_api_t);
    if (post == 0) {
        return -WZ_ENOSYS;
    }

    req.address = (uint32_t)(uintptr_t)line;
    req.done_task_id = 0U;
    req.done_event_id = 0U;

    return post(APPLICATION_TASK_AT_CLI,
                APPLICATION_AT_CLI_EXEC_LINE,
                &req,
                sizeof(req));
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_AT_CLI_API_H_ */
