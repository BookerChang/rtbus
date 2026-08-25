/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_ABI_H_
#define RUNTIME_ABI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APPLICATION_CTX_APP_BITS     28U
#define APPLICATION_CTX_SYS_NONE     0U
#define APPLICATION_CTX_APP_MASK     ((1UL << APPLICATION_CTX_APP_BITS) - 1UL)
#define APPLICATION_APP_EVT(app_id)  ((uint32_t)(APPLICATION_CTX_SYS_NONE << 28U) | \
                                      ((uint32_t)(app_id) & APPLICATION_CTX_APP_MASK))
#define APPLICATION_LORAWAN_SEND_HEADER_SIZE 8U
#define APPLICATION_LORAWAN_SEND_PAYLOAD_MAX 24U
#define APPLICATION_VALIDATION_ADD_PAYLOAD_SIZE 12U
#define APPLICATION_AT_CLI_EXEC_LINE_MAX 256U
#define APPLICATION_BLE_SET_SECURITY_PAYLOAD_SIZE 12U

enum runtime_gpio_mode {
    RUNTIME_GPIO_MODE_INPUT = 0,
    RUNTIME_GPIO_MODE_OUTPUT,
    RUNTIME_GPIO_MODE_INPUT_PULLUP,
    RUNTIME_GPIO_MODE_INPUT_PULLDOWN,
};

#define WISNODEZ_GPIO_MODE_INPUT          RUNTIME_GPIO_MODE_INPUT
#define WISNODEZ_GPIO_MODE_OUTPUT         RUNTIME_GPIO_MODE_OUTPUT
#define WISNODEZ_GPIO_MODE_INPUT_PULLUP   RUNTIME_GPIO_MODE_INPUT_PULLUP
#define WISNODEZ_GPIO_MODE_INPUT_PULLDOWN RUNTIME_GPIO_MODE_INPUT_PULLDOWN

enum application_task_id {
    APPLICATION_TASK_DEMO1 = 0,
    APPLICATION_TASK_DEMO2,
    APPLICATION_TASK_AT_CLI,
    APPLICATION_TASK_LORAWAN_SERVICE,
    APPLICATION_TASK_VALIDATION_SERVICE,
    APPLICATION_TASK_DFU_SERVICE,
    APPLICATION_TASK_BLE_SERVICE,
    APPLICATION_TASK_NATIVE_BOOTSTRAP,
};

enum application_lorawan_eid {
    APPLICATION_LORAWAN_EID_NONE = 0,
    APPLICATION_LORAWAN_EID_SEND,
};

#define APPLICATION_LORAWAN_SEND \
    APPLICATION_APP_EVT(APPLICATION_LORAWAN_EID_SEND)

enum application_at_cli_eid {
    APPLICATION_AT_CLI_EID_NONE = 0,
    APPLICATION_AT_CLI_EID_EXEC_LINE,
};

#define APPLICATION_AT_CLI_EXEC_LINE \
    APPLICATION_APP_EVT(APPLICATION_AT_CLI_EID_EXEC_LINE)

enum application_validation_eid {
    APPLICATION_VALIDATION_EID_NONE = 0,
    APPLICATION_VALIDATION_EID_ADD,
};

#define APPLICATION_VALIDATION_ADD \
    APPLICATION_APP_EVT(APPLICATION_VALIDATION_EID_ADD)

enum application_dfu_eid {
    APPLICATION_DFU_EID_NONE = 0,
    APPLICATION_DFU_EID_PATCH_STAGED,
    APPLICATION_DFU_EID_APP_STAGED,
    APPLICATION_DFU_EID_CONFIG_STAGED,
    APPLICATION_DFU_EID_CONFIG_STEP_DONE,
    APPLICATION_DFU_EID_IMAGE_STAGED,
};

enum application_dfu_source {
    APPLICATION_DFU_SOURCE_USER = 0,
    APPLICATION_DFU_SOURCE_FUOTA,
};

#define APPLICATION_DFU_PATCH_STAGED \
    APPLICATION_APP_EVT(APPLICATION_DFU_EID_PATCH_STAGED)
#define APPLICATION_DFU_APP_STAGED \
    APPLICATION_APP_EVT(APPLICATION_DFU_EID_APP_STAGED)
#define APPLICATION_DFU_CONFIG_STAGED \
    APPLICATION_APP_EVT(APPLICATION_DFU_EID_CONFIG_STAGED)
#define APPLICATION_DFU_CONFIG_STEP_DONE \
    APPLICATION_APP_EVT(APPLICATION_DFU_EID_CONFIG_STEP_DONE)
#define APPLICATION_DFU_IMAGE_STAGED \
    APPLICATION_APP_EVT(APPLICATION_DFU_EID_IMAGE_STAGED)

enum application_ble_eid {
    APPLICATION_BLE_EID_NONE = 0,
    APPLICATION_BLE_EID_ADV_START,
    APPLICATION_BLE_EID_SET_SECURITY,
};

#define APPLICATION_BLE_ADV_START \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_ADV_START)
#define APPLICATION_BLE_SET_SECURITY \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_SET_SECURITY)

struct application_dfu_staged_req {
    uint32_t source;
    uint32_t address;
    uint32_t size;
};

#define application_dfu_patch_staged_req application_dfu_staged_req
#define application_dfu_config_staged_req application_dfu_staged_req
#define application_dfu_app_staged_req application_dfu_staged_req
#define application_dfu_image_staged_req application_dfu_staged_req

struct application_at_cli_exec_line_req {
    uint32_t address;
    uint32_t done_task_id;
    uint32_t done_event_id;
};

struct application_at_cli_exec_line_done_req {
    int32_t status;
};

struct application_lorawan_send_req {
    uint8_t port;
    uint8_t confirmed;
    uint8_t len;
    uint8_t reserved;
    uint32_t result_addr;
    uint8_t data[APPLICATION_LORAWAN_SEND_PAYLOAD_MAX];
};

struct application_validation_add_req {
    uint32_t result_addr;
    int32_t lhs;
    int32_t rhs;
};

struct application_ble_set_security_req {
    uint32_t result_addr;
    uint32_t conn;
    uint8_t level;
    uint8_t reserved[3];
};

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_ABI_H_ */
