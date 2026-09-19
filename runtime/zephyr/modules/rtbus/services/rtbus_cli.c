/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/iterable_sections.h>

#include <rtbus.h>
#include <rtbus/cli.h>
#include <rtbus/native_service.h>
#include <rtbus/ymodem.h>

#include "runtime_abi.h"

LOG_MODULE_REGISTER(rtbus_cli, LOG_LEVEL_INF);

#define RTBUS_CLI_HEADER "@RTBUS:"

static char rtbus_cli_line[RTBUS_CLI_LINE_MAX];
static size_t rtbus_cli_line_len;
static atomic_t rtbus_cli_input_enabled = ATOMIC_INIT(1);
static atomic_t rtbus_cli_input_pending;
static void rtbus_cli_input_work_handler(struct k_work *work);

K_MSGQ_DEFINE(rtbus_cli_input_msgq, sizeof(char), 128, 1);
K_WORK_DEFINE(rtbus_cli_input_work, rtbus_cli_input_work_handler);

static int rtbus_cli_test_handler(const char *args)
{
    int ret;

    ARG_UNUSED(args);

    ret = rtbus_post(RTBUS_TASK_DIAGNOSTICS,
                     APPLICATION_DIAGNOSTICS_CLI,
                     NULL,
                     0U);
    if (ret != 0) {
        LOG_ERR("Diagnostics test event post failed: %d", ret);
    }

    return ret;
}

RTBUS_CLI_COMMAND_DEFINE(rtbus_cli_test_command,
                         "TEST",
                         rtbus_cli_test_handler);

static int rtbus_cli_dfu_handler(const char *args)
{
    int ret;

    if (strcmp(args, "APP") != 0) {
        LOG_ERR("Unsupported DFU target: %s", args);
        return -EINVAL;
    }

    native_service_suppress_console(true);
    atomic_set(&rtbus_cli_input_enabled, 0);
    ret = native_service_stop_for_upgrade();
    if (ret != 0) {
        atomic_set(&rtbus_cli_input_enabled, 1);
        native_service_suppress_console(false);
        LOG_ERR("Native service stop for DFU failed: %d", ret);
        return ret;
    }

    ret = rtbus_ymodem_recv(RTBUS_SERIAL_PORT_0);
    atomic_set(&rtbus_cli_input_enabled, 1);
    native_service_resume_after_upgrade();

    if (ret != 0) {
        LOG_ERR("YMODEM receive test failed: %d", ret);
    }

    return ret;
}

RTBUS_CLI_COMMAND_DEFINE(rtbus_cli_dfu_command,
                         "DFU",
                         rtbus_cli_dfu_handler);

static int rtbus_cli_reboot_handler(const char *args)
{
    if (args[0] != '\0') {
        LOG_ERR("Unsupported REBOOT args: %s", args);
        return -EINVAL;
    }

    LOG_INF("RTBus CLI reboot requested");
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

RTBUS_CLI_COMMAND_DEFINE(rtbus_cli_reboot_command,
                         "REBOOT",
                         rtbus_cli_reboot_handler);

static void rtbus_cli_dispatch_line(void)
{
    char *command_name;
    const char *args = "";
    char *separator;

    if (rtbus_cli_line_len == 0U) {
        return;
    }

    rtbus_cli_line[rtbus_cli_line_len] = '\0';
    if (strncmp(rtbus_cli_line, RTBUS_CLI_HEADER,
                sizeof(RTBUS_CLI_HEADER) - 1U) != 0) {
        LOG_INF("RTBus CLI ignored line: %s", rtbus_cli_line);
        rtbus_cli_line_len = 0U;
        return;
    }

    command_name = &rtbus_cli_line[sizeof(RTBUS_CLI_HEADER) - 1U];
    separator = strchr(command_name, '=');
    if (separator != NULL) {
        *separator = '\0';
        args = separator + 1;
    }

    STRUCT_SECTION_FOREACH(rtbus_cli_command, command) {
        if (command->name != NULL && command->handler != NULL &&
            strcmp(command_name, command->name) == 0) {
            (void)command->handler(args);
            rtbus_cli_line_len = 0U;
            return;
        }
    }

    LOG_INF("RTBus CLI command: %s", command_name);

    rtbus_cli_line_len = 0U;
}

static void rtbus_cli_process_byte(char byte)
{
    if (byte == '\r' || byte == '\n') {
        rtbus_cli_dispatch_line();
        return;
    }

    if (rtbus_cli_line_len >= (sizeof(rtbus_cli_line) - 1U)) {
        LOG_WRN("RTBus CLI line too long");
        rtbus_cli_line_len = 0U;
        return;
    }

    rtbus_cli_line[rtbus_cli_line_len++] = byte;
}

void rtbus_cli_submit_byte(char byte)
{
    if (atomic_get(&rtbus_cli_input_enabled) == 0) {
        return;
    }

    if (k_msgq_put(&rtbus_cli_input_msgq, &byte, K_NO_WAIT) != 0) {
        return;
    }

    if (atomic_cas(&rtbus_cli_input_pending, 0, 1)) {
        (void)k_work_submit(&rtbus_cli_input_work);
    }
}

static void rtbus_cli_input_work_handler(struct k_work *work)
{
    int ret;

    ARG_UNUSED(work);

    ret = rtbus_post(RTBUS_TASK_CLI, RTBUS_CLI_INPUT, NULL, 0U);
    if (ret != 0) {
        atomic_set(&rtbus_cli_input_pending, 0);
    }
}

static void rtbus_cli_process_input(void)
{
    char byte;

    while (k_msgq_get(&rtbus_cli_input_msgq, &byte, K_NO_WAIT) == 0) {
        rtbus_cli_process_byte(byte);
    }

    atomic_set(&rtbus_cli_input_pending, 0);

    if (k_msgq_num_used_get(&rtbus_cli_input_msgq) != 0 &&
        atomic_cas(&rtbus_cli_input_pending, 0, 1)) {
        (void)k_work_submit(&rtbus_cli_input_work);
    }
}

static uint8_t rtbus_cli_process(rtbus_ctx_t *ctx)
{
    switch (ctx->id) {
        case RTBUS_CLI_INPUT:
            rtbus_cli_process_input();
            break;

        default:
            break;
    }

    return 0;
}

RTBUS_TASK_REGISTER(rtbus_cli_task,
                    RTBUS_TASK_CLI,
                    RTBUS_TASK_PRIORITY_LOW,
                    rtbus_cli_process);
