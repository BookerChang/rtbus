/*
 * SPDX-License-Identifier: Apache-2.0
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

typedef uintptr_t runtime_ble_conn_t;

static inline int runtime_ble_adv_start(void)
{
    runtime_rtbus_post_api_t post;

    post = WZ_API_FN(WISNODEZ_API_SLOT_RTBUS_POST,
                     runtime_rtbus_post_api_t);
    if (post == 0) {
        return -WZ_ENOSYS;
    }

    return post(APPLICATION_TASK_BLE_SERVICE,
                APPLICATION_BLE_ADV_START,
                0,
                0U);
}

static inline int runtime_ble_set_security(runtime_ble_conn_t conn,
                                           uint8_t level)
{
    struct application_ble_set_security_req req;
    /* Keep the completion result in application RAM for runtime write-back. */
    static volatile int32_t result;

    req.result_addr = (uint32_t)(uintptr_t)&result;
    req.conn = (uint32_t)conn;
    req.level = level;
    req.reserved[0] = 0U;
    req.reserved[1] = 0U;
    req.reserved[2] = 0U;

    return runtime_post_wait_result(APPLICATION_TASK_BLE_SERVICE,
                                    APPLICATION_BLE_SET_SECURITY,
                                    &req,
                                    APPLICATION_BLE_SET_SECURITY_PAYLOAD_SIZE,
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
