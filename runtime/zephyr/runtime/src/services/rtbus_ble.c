/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif
#include <zephyr/sys/util.h>

#include <rtbus.h>
#include <rtbus/native_service.h>

#include "runtime_abi.h"
#include "runtime_ble_api.h"

LOG_MODULE_REGISTER(rtbus_ble, LOG_LEVEL_INF);

static const struct bt_data rtbus_ble_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data rtbus_ble_sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static bool rtbus_ble_ready;
static bool rtbus_ble_advertising;
static uint32_t rtbus_ble_current_conn;

static void rtbus_ble_store_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static void rtbus_ble_connected(struct bt_conn *conn, uint8_t err)
{
    uint8_t payload[4];

    rtbus_ble_advertising = false;

    if (err != 0) {
        rtbus_ble_current_conn = 0U;
        LOG_ERR("BLE connection failed: 0x%02x %s",
                err, bt_hci_err_to_str(err));
        (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                        RUNTIME_EVENT_BLE_CONNECT_FAILED,
                                        &err,
                                        sizeof(err));
        return;
    }

    rtbus_ble_current_conn = (uint32_t)(uintptr_t)conn;
    rtbus_ble_store_u32_le(payload, rtbus_ble_current_conn);

    LOG_INF("BLE connected");
    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_CONNECTED,
                                    payload,
                                    sizeof(payload));
}

static void rtbus_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (rtbus_ble_current_conn == (uint32_t)(uintptr_t)conn) {
        rtbus_ble_current_conn = 0U;
    }

    LOG_INF("BLE disconnected: 0x%02x %s",
            reason, bt_hci_err_to_str(reason));
    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_DISCONNECTED,
                                    &reason,
                                    sizeof(reason));
}

static void rtbus_ble_recycled(void)
{
    rtbus_ble_current_conn = 0U;
    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_RECYCLED,
                                    NULL,
                                    0U);
}

#if defined(CONFIG_BT_SMP)
static void rtbus_ble_security_changed(struct bt_conn *conn,
                                       bt_security_t level,
                                       enum bt_security_err err)
{
    uint8_t payload[2];

    ARG_UNUSED(conn);

    payload[0] = (uint8_t)level;
    payload[1] = (uint8_t)err;
    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_SECURITY_CHANGED,
                                    payload,
                                    sizeof(payload));

    if (err != BT_SECURITY_ERR_SUCCESS) {
        LOG_ERR("BLE security failed: level %u err %u",
                (unsigned int)level, (unsigned int)err);
        return;
    }

    LOG_INF("BLE security enabled: level %u", (unsigned int)level);
}
#endif

static int rtbus_ble_set_security_now(uint32_t conn_handle, uint8_t level)
{
#if defined(CONFIG_BT_SMP)
    struct bt_conn *conn;
    int ret;

    if (conn_handle == 0U || rtbus_ble_current_conn == 0U ||
        conn_handle != rtbus_ble_current_conn) {
        return -ENOTCONN;
    }

    conn = (struct bt_conn *)(uintptr_t)conn_handle;
    ret = bt_conn_set_security(conn, (bt_security_t)level);
    if (ret != 0) {
        LOG_ERR("BLE security request failed: %d", ret);
        return ret;
    }

    LOG_INF("BLE security requested: level %u", (unsigned int)level);
    return 0;
#else
    ARG_UNUSED(conn_handle);
    ARG_UNUSED(level);

    return -ENOTSUP;
#endif
}

static int rtbus_ble_pair_now(uint32_t conn_handle)
{
    return rtbus_ble_set_security_now(conn_handle, BT_SECURITY_L2);
}

BT_CONN_CB_DEFINE(rtbus_ble_conn_callbacks) = {
    .connected = rtbus_ble_connected,
    .disconnected = rtbus_ble_disconnected,
    .recycled = rtbus_ble_recycled,
#if defined(CONFIG_BT_SMP)
    .security_changed = rtbus_ble_security_changed,
#endif
};

static int rtbus_ble_adv_start_now(void)
{
    int ret;

    if (!rtbus_ble_ready) {
        ret = bt_enable(NULL);
        if (ret != 0) {
            LOG_ERR("BLE enable failed: %d", ret);
            return ret;
        }

#if defined(CONFIG_SETTINGS)
        ret = settings_load();
        if (ret != 0) {
            LOG_ERR("BLE settings load failed: %d", ret);
            return ret;
        }

        LOG_INF("BLE settings loaded");
#endif

        rtbus_ble_ready = true;
    }

    if (rtbus_ble_advertising) {
        return 0;
    }

    ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                          rtbus_ble_ad,
                          ARRAY_SIZE(rtbus_ble_ad),
                          rtbus_ble_sd,
                          ARRAY_SIZE(rtbus_ble_sd));
    if (ret != 0) {
        LOG_ERR("BLE advertising start failed: %d", ret);
        return ret;
    }

    rtbus_ble_advertising = true;
    LOG_INF("BLE advertising started");
    return 0;
}

static uint8_t rtbus_ble_process(rtbus_ctx_t *ctx)
{
    const struct application_ble_pair_req *pair_req;
    const struct application_ble_set_security_req *security_req;
    int ret;

    switch (ctx->id) {
        case RTBUS_SYS_EVT(RTBUS_SYS_INIT):
            rtbus_ble_ready = false;
            rtbus_ble_advertising = false;
            rtbus_ble_current_conn = 0U;
            break;

        case APPLICATION_BLE_ADV_START:
            (void)rtbus_ble_adv_start_now();
            break;

        case APPLICATION_BLE_PAIR:
            if (ctx->payload_len != APPLICATION_BLE_PAIR_PAYLOAD_SIZE) {
                break;
            }

            pair_req = (const struct application_ble_pair_req *)ctx->payload;
            ret = rtbus_ble_pair_now(pair_req->conn);
            if (native_service_complete_result(pair_req->result_addr,
                                               ret) != 0) {
                LOG_ERR("BLE pairing result write failed: 0x%08x",
                        pair_req->result_addr);
            }
            break;

        case APPLICATION_BLE_SET_SECURITY:
            if (ctx->payload_len != APPLICATION_BLE_SET_SECURITY_PAYLOAD_SIZE) {
                break;
            }

            security_req =
                (const struct application_ble_set_security_req *)ctx->payload;
            ret = rtbus_ble_set_security_now(security_req->conn,
                                             security_req->level);
            if (native_service_complete_result(security_req->result_addr,
                                               ret) != 0) {
                LOG_ERR("BLE security result write failed: 0x%08x",
                        security_req->result_addr);
            }
            break;

        default:
            break;
    }

    return 0;
}

RTBUS_TASK_REGISTER(rtbus_ble_task,
                    RTBUS_TASK_RUNTIME_BLE,
                    RTBUS_TASK_PRIORITY_LOW,
                    rtbus_ble_process);
