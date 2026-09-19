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
#define APPLICATION_APP_EVT(app_id)  ((uint32_t)(APPLICATION_CTX_SYS_NONE << 28U) | ((uint32_t)(app_id) & APPLICATION_CTX_APP_MASK))
#define APPLICATION_LORAWAN_SEND_HEADER_SIZE        8U
#define APPLICATION_LORAWAN_SEND_PAYLOAD_MAX        24U
#define APPLICATION_DIAGNOSTICS_ADD_PAYLOAD_SIZE    12U
#define APPLICATION_RUNTIME_SERIAL_BEGIN_PAYLOAD_SIZE 12U
#define APPLICATION_BLE_PAIR_PAYLOAD_SIZE           8U
#define APPLICATION_BLE_SET_SECURITY_PAYLOAD_SIZE   12U
#define APPLICATION_BLE_GAP_INIT_PAYLOAD_SIZE       8U
#define APPLICATION_BLE_GATT_NOTIFY_REGISTER_PAYLOAD_SIZE 8U
#define APPLICATION_BLE_GATT_VALUE_SET_PAYLOAD_SIZE \
    ((uint16_t)sizeof(struct application_ble_gatt_value_set_req))
#define RTBUS_CLI_LINE_MAX                          256U
#define RTBUS_SERIAL_PORT_0                         0U
#define RTBUS_SERIAL_PORT_1                         1U
#define RUNTIME_BLE_GATT_VALUE_MAX                  32U
#define RUNTIME_BLE_GAP_DATA_MAX                    4U
#define RUNTIME_BLE_GAP_DATA_VALUE_MAX              31U

enum runtime_gpio_mode {
    RUNTIME_GPIO_MODE_INPUT = 0,
    RUNTIME_GPIO_MODE_OUTPUT,
    RUNTIME_GPIO_MODE_INPUT_PULLUP,
    RUNTIME_GPIO_MODE_INPUT_PULLDOWN,
};

enum rtbus_task_id {
    RTBUS_TASK_DIAGNOSTICS = 0,
    RTBUS_TASK_CLI,
    RTBUS_TASK_RUNTIME_SERIAL,
    RTBUS_TASK_RUNTIME_BLE,
};

enum rtbus_cli_eid {
    RTBUS_CLI_EID_NONE = 0,
    RTBUS_CLI_EID_INPUT,
};

#define RTBUS_CLI_INPUT \
    APPLICATION_APP_EVT(RTBUS_CLI_EID_INPUT)

enum application_runtime_serial_eid {
    APPLICATION_RUNTIME_SERIAL_EID_NONE = 0,
    APPLICATION_RUNTIME_SERIAL_EID_BEGIN,
};

#define APPLICATION_RUNTIME_SERIAL_BEGIN \
    APPLICATION_APP_EVT(APPLICATION_RUNTIME_SERIAL_EID_BEGIN)

enum application_ble_eid {
    APPLICATION_BLE_EID_NONE = 0,
    APPLICATION_BLE_EID_ADV_START,
    APPLICATION_BLE_EID_PAIR,
    APPLICATION_BLE_EID_SET_SECURITY,
    APPLICATION_BLE_EID_GAP_INIT,
    APPLICATION_BLE_EID_GATT_NOTIFY_REGISTER,
    APPLICATION_BLE_EID_GATT_VALUE_SET,
};

#define APPLICATION_BLE_ADV_START \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_ADV_START)
#define APPLICATION_BLE_PAIR \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_PAIR)
#define APPLICATION_BLE_SET_SECURITY \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_SET_SECURITY)
#define APPLICATION_BLE_GAP_INIT \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_GAP_INIT)
#define APPLICATION_BLE_GATT_NOTIFY_REGISTER \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_GATT_NOTIFY_REGISTER)
#define APPLICATION_BLE_GATT_VALUE_SET \
    APPLICATION_APP_EVT(APPLICATION_BLE_EID_GATT_VALUE_SET)

#define APPLICATION_LORAWAN_SEND \
    APPLICATION_APP_EVT(APPLICATION_LORAWAN_EID_SEND)

enum application_diagnostics_eid {
    APPLICATION_DIAGNOSTICS_EID_NONE = 0,
    APPLICATION_DIAGNOSTICS_EID_ADD,
    APPLICATION_DIAGNOSTICS_EID_CLI,
};

#define APPLICATION_DIAGNOSTICS_ADD \
    APPLICATION_APP_EVT(APPLICATION_DIAGNOSTICS_EID_ADD)
#define APPLICATION_DIAGNOSTICS_CLI \
    APPLICATION_APP_EVT(APPLICATION_DIAGNOSTICS_EID_CLI)

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

struct application_diagnostics_add_req {
    uint32_t result_addr;
    int32_t lhs;
    int32_t rhs;
};

struct application_runtime_serial_begin_req {
    uint32_t result_addr;
    uint32_t port;
    uint32_t baud;
};

struct application_ble_pair_req {
    uint32_t result_addr;
    uint32_t conn;
};

struct application_ble_set_security_req {
    uint32_t result_addr;
    uint32_t conn;
    uint8_t level;
    uint8_t reserved[3];
};

struct runtime_ble_gap_def {
    uint8_t security_level;
    uint8_t flags;
    uint8_t adv_data_count;
    uint8_t scan_data_count;
    uint32_t adv_data_addr;
    uint32_t scan_data_addr;
};

struct runtime_ble_gap_data_def {
    uint8_t type;
    uint8_t data_len;
    uint8_t data[RUNTIME_BLE_GAP_DATA_VALUE_MAX];
};

struct application_ble_gap_init_req {
    uint32_t result_addr;
    uint32_t def_addr;
};

struct runtime_ble_gatt_chrc_def {
    uint8_t value_uuid[16];
    uint8_t value_len;
    uint8_t properties;
    uint8_t value_perm;
    uint8_t ccc_perm;
    uint32_t value_addr;
};

struct runtime_ble_gatt_service_def {
    uint8_t service_uuid[16];
    uint8_t chrc_count;
    uint8_t reserved[3];
    uint32_t chrcs_addr;
};

struct runtime_ble_gatt_notify_def {
    uint8_t service_uuid[16];
    struct runtime_ble_gatt_chrc_def chrc;
};

struct application_ble_gatt_notify_register_req {
    uint32_t result_addr;
    uint32_t def_addr;
};

struct application_ble_gatt_value_set_req {
    uint32_t result_addr;
    uint8_t handle;
    uint8_t chrc;
    uint8_t value_len;
    uint8_t reserved;
    uint32_t value_addr;
};

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_ABI_H_ */
