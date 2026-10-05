/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_CLI_H_
#define RTBUS_CLI_H_

#include <zephyr/sys/iterable_sections.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtbus_cli_command {
    const char *name;
    int (*handler)(const char *args);
};

void rtbus_cli_submit_byte(char byte);

#define RTBUS_CLI_COMMAND_DEFINE(_name, _command, _handler)                \
    const STRUCT_SECTION_ITERABLE(rtbus_cli_command, _name) = {            \
        .name = (_command),                                                \
        .handler = (_handler),                                             \
    }

#ifdef __cplusplus
}
#endif

#endif /* RTBUS_CLI_H_ */
