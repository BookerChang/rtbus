/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RUNTIME_BLE_API_H_
#define RUNTIME_BLE_API_H_

#include "runtime_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTIME_EVENT_GROUP_BLE            1U
#define RUNTIME_EVENT_BLE_CONNECTED        1U
#define RUNTIME_EVENT_BLE_DISCONNECTED     2U
#define RUNTIME_EVENT_BLE_RECYCLED         3U
#define RUNTIME_EVENT_BLE_SECURITY_CHANGED 4U
#define RUNTIME_EVENT_BLE_CONNECT_FAILED   5U

#define RUNTIME_BLE_SECURITY_NONE          0U
#define RUNTIME_BLE_SECURITY_L2            2U

typedef uintptr_t runtime_ble_conn_t;
typedef uint8_t runtime_ble_gatt_handle_t;
typedef uint8_t runtime_ble_gatt_chrc_handle_t;

#define RUNTIME_BLE_GATT_CHRC_READ      0x02U
#define RUNTIME_BLE_GATT_CHRC_NOTIFY    0x10U

#define RUNTIME_BLE_GATT_PERM_READ      0x01U
#define RUNTIME_BLE_GATT_PERM_WRITE     0x02U
#define RUNTIME_BLE_GATT_PERM_READ_ENCRYPT  0x04U
#define RUNTIME_BLE_GATT_PERM_WRITE_ENCRYPT 0x08U
#define RUNTIME_BLE_GATT_PERM_READ_AUTHEN   0x10U
#define RUNTIME_BLE_GATT_PERM_WRITE_AUTHEN  0x20U

#define RUNTIME_BLE_GAP_AD_FLAGS            0x01U
#define RUNTIME_BLE_GAP_AD_UUID128_ALL      0x07U
#define RUNTIME_BLE_GAP_AD_NAME_COMPLETE    0x09U

#define RUNTIME_BLE_GAP_FLAG_GENERAL        0x02U
#define RUNTIME_BLE_GAP_FLAG_NO_BREDR       0x04U

#define RUNTIME_BLE_UUID128_ENCODE(w32, w1, w2, w3, w48) \
    ((uint8_t)((uint64_t)(w48))), \
    ((uint8_t)((uint64_t)(w48) >> 8)), \
    ((uint8_t)((uint64_t)(w48) >> 16)), \
    ((uint8_t)((uint64_t)(w48) >> 24)), \
    ((uint8_t)((uint64_t)(w48) >> 32)), \
    ((uint8_t)((uint64_t)(w48) >> 40)), \
    ((uint8_t)((uint16_t)(w3))), \
    ((uint8_t)((uint16_t)(w3) >> 8)), \
    ((uint8_t)((uint16_t)(w2))), \
    ((uint8_t)((uint16_t)(w2) >> 8)), \
    ((uint8_t)((uint16_t)(w1))), \
    ((uint8_t)((uint16_t)(w1) >> 8)), \
    ((uint8_t)((uint32_t)(w32))), \
    ((uint8_t)((uint32_t)(w32) >> 8)), \
    ((uint8_t)((uint32_t)(w32) >> 16)), \
    ((uint8_t)((uint32_t)(w32) >> 24))

static inline int runtime_ble_adv_start(void)
{
    return rtbus_post(RTBUS_TASK_RUNTIME_BLE,
                      APPLICATION_BLE_ADV_START,
                      0,
                      0U);
}

static inline int runtime_ble_gap_init(const struct runtime_ble_gap_def *def)
{
    struct application_ble_gap_init_req req;
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.def_addr = (uint32_t)(uintptr_t)def;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_BLE,
                                  APPLICATION_BLE_GAP_INIT,
                                  &req,
                                  APPLICATION_BLE_GAP_INIT_PAYLOAD_SIZE,
                                  &result);
}

static inline int runtime_ble_set_security(runtime_ble_conn_t conn,
                                           uint8_t level)
{
    struct application_ble_set_security_req req;
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.conn = (uint32_t)conn;
    req.level = level;
    req.reserved[0] = 0U;
    req.reserved[1] = 0U;
    req.reserved[2] = 0U;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_BLE,
                                  APPLICATION_BLE_SET_SECURITY,
                                  &req,
                                  APPLICATION_BLE_SET_SECURITY_PAYLOAD_SIZE,
                                  &result);
}

static inline int runtime_ble_pair(runtime_ble_conn_t conn)
{
    struct application_ble_pair_req req;
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.conn = (uint32_t)conn;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_BLE,
                                  APPLICATION_BLE_PAIR,
                                  &req,
                                  APPLICATION_BLE_PAIR_PAYLOAD_SIZE,
                                  &result);
}

static inline int runtime_ble_gatt_service_register(
    const struct runtime_ble_gatt_service_def *def)
{
    struct application_ble_gatt_notify_register_req req;
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.def_addr = (uint32_t)(uintptr_t)def;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_BLE,
                                  APPLICATION_BLE_GATT_NOTIFY_REGISTER,
                                  &req,
                                  APPLICATION_BLE_GATT_NOTIFY_REGISTER_PAYLOAD_SIZE,
                                  &result);
}

static inline int runtime_ble_gatt_notify_register(
    const struct runtime_ble_gatt_notify_def *def)
{
    struct runtime_ble_gatt_service_def service_def;

    service_def.chrc_count = 1U;
    service_def.reserved[0] = 0U;
    service_def.reserved[1] = 0U;
    service_def.reserved[2] = 0U;
    service_def.chrcs_addr = (uint32_t)(uintptr_t)&def->chrc;

    for (uint8_t i = 0U; i < sizeof(service_def.service_uuid); i++) {
        service_def.service_uuid[i] = def->service_uuid[i];
    }

    return runtime_ble_gatt_service_register(&service_def);
}

static inline int runtime_ble_gatt_value_set(runtime_ble_gatt_handle_t handle,
                                             runtime_ble_gatt_chrc_handle_t chrc,
                                             const void *value,
                                             uint8_t value_len)
{
    struct application_ble_gatt_value_set_req req;
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.handle = handle;
    req.chrc = chrc;
    req.value_len = value_len;
    req.reserved = 0U;
    req.value_addr = (uint32_t)(uintptr_t)value;

    return rtbus_post_wait_result(RTBUS_TASK_RUNTIME_BLE,
                                  APPLICATION_BLE_GATT_VALUE_SET,
                                  &req,
                                  APPLICATION_BLE_GATT_VALUE_SET_PAYLOAD_SIZE,
                                  &result);
}

static inline int runtime_event_is_ble_disconnected(
    const struct runtime_event_tlv *event,
    uint8_t *reason)
{
    if (event == 0 || reason == 0 ||
        event->group != RUNTIME_EVENT_GROUP_BLE ||
        event->tag != RUNTIME_EVENT_BLE_DISCONNECTED ||
        event->len != 1U) {
        return 0;
    }

    *reason = event->value[0];
    return 1;
}

static inline int runtime_event_is_ble_connect_failed(
    const struct runtime_event_tlv *event,
    uint8_t *err)
{
    if (event == 0 || err == 0 ||
        event->group != RUNTIME_EVENT_GROUP_BLE ||
        event->tag != RUNTIME_EVENT_BLE_CONNECT_FAILED ||
        event->len != 1U) {
        return 0;
    }

    *err = event->value[0];
    return 1;
}

static inline int runtime_event_is_ble_connected(
    const struct runtime_event_tlv *event,
    runtime_ble_conn_t *conn)
{
    if (event == 0 || conn == 0 ||
        event->group != RUNTIME_EVENT_GROUP_BLE ||
        event->tag != RUNTIME_EVENT_BLE_CONNECTED ||
        event->len != 4U) {
        return 0;
    }

    *conn = (runtime_ble_conn_t)(
        ((uint32_t)event->value[0]) |
        ((uint32_t)event->value[1] << 8) |
        ((uint32_t)event->value[2] << 16) |
        ((uint32_t)event->value[3] << 24));
    return 1;
}

static inline int runtime_event_is_ble_recycled(
    const struct runtime_event_tlv *event)
{
    if (event == 0 ||
        event->group != RUNTIME_EVENT_GROUP_BLE ||
        event->tag != RUNTIME_EVENT_BLE_RECYCLED ||
        event->len != 0U) {
        return 0;
    }

    return 1;
}

static inline int runtime_event_is_ble_security_changed(
    const struct runtime_event_tlv *event,
    uint8_t *level,
    uint8_t *err)
{
    if (event == 0 || level == 0 || err == 0 ||
        event->group != RUNTIME_EVENT_GROUP_BLE ||
        event->tag != RUNTIME_EVENT_BLE_SECURITY_CHANGED ||
        event->len != 2U) {
        return 0;
    }

    *level = event->value[0];
    *err = event->value[1];
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_BLE_API_H_ */
