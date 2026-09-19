/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
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

#ifndef RTBUS_BLE_GATT_TRACE
#define RTBUS_BLE_GATT_TRACE 0
#endif

static uint8_t rtbus_ble_ad_service_uuid[16];
static bool rtbus_ble_ad_has_service_uuid;
static struct bt_data rtbus_ble_app_ad[RUNTIME_BLE_GAP_DATA_MAX];
static uint8_t rtbus_ble_app_ad_values[RUNTIME_BLE_GAP_DATA_MAX]
                                      [RUNTIME_BLE_GAP_DATA_VALUE_MAX];
static uint8_t rtbus_ble_app_ad_count;
static struct bt_data rtbus_ble_app_sd[RUNTIME_BLE_GAP_DATA_MAX];
static uint8_t rtbus_ble_app_sd_values[RUNTIME_BLE_GAP_DATA_MAX]
                                      [RUNTIME_BLE_GAP_DATA_VALUE_MAX];
static uint8_t rtbus_ble_app_sd_count;

static const struct bt_data rtbus_ble_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_UUID128_ALL,
            rtbus_ble_ad_service_uuid,
            sizeof(rtbus_ble_ad_service_uuid)),
};

static const struct bt_data rtbus_ble_sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_uuid_16 rtbus_ble_uuid_gatt_primary =
    BT_UUID_INIT_16(BT_UUID_GATT_PRIMARY_VAL);
static const struct bt_uuid_16 rtbus_ble_uuid_gatt_chrc =
    BT_UUID_INIT_16(BT_UUID_GATT_CHRC_VAL);
static const struct bt_uuid_16 rtbus_ble_uuid_gatt_ccc =
    BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

static bool rtbus_ble_ready;
static bool rtbus_ble_advertising;
static uint32_t rtbus_ble_current_conn;
static uint8_t rtbus_ble_gap_security_level;
static uint8_t rtbus_ble_gap_flags;

#define RTBUS_BLE_APP_GATT_MAX 2U
#define RTBUS_BLE_APP_GATT_CHRC_MAX 4U
#define RTBUS_BLE_APP_GATT_ATTR_MAX \
    (1U + (RTBUS_BLE_APP_GATT_CHRC_MAX * 3U))

struct rtbus_ble_app_gatt_chrc {
    bool notify_enabled;
    bool has_ccc;
    uint8_t value_attr_index;
    uint8_t ccc_attr_index;
    uint8_t value_len;
    struct bt_uuid_128 value_uuid;
    struct bt_gatt_chrc value_chrc;
    struct bt_gatt_ccc_managed_user_data value_ccc;
    uint8_t value[RUNTIME_BLE_GATT_VALUE_MAX];
};

struct rtbus_ble_app_gatt {
    bool prepared;
    bool registered;
    uint8_t attr_count;
    uint8_t chrc_count;
    struct bt_uuid_128 service_uuid;
    struct rtbus_ble_app_gatt_chrc chrcs[RTBUS_BLE_APP_GATT_CHRC_MAX];
    struct bt_gatt_attr attrs[RTBUS_BLE_APP_GATT_ATTR_MAX];
    struct bt_gatt_service service;
};

static struct rtbus_ble_app_gatt rtbus_ble_app_gatts[RTBUS_BLE_APP_GATT_MAX];
static uint8_t rtbus_ble_app_gatt_count;

static ssize_t rtbus_ble_app_gatt_read(struct bt_conn *conn,
                                       const struct bt_gatt_attr *attr,
                                       void *buf,
                                       uint16_t len,
                                       uint16_t offset)
{
    const struct rtbus_ble_app_gatt_chrc *chrc = attr->user_data;

    if (chrc == NULL) {
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             chrc->value, chrc->value_len);
}

static struct rtbus_ble_app_gatt_chrc *rtbus_ble_app_gatt_find_chrc_ccc(
    const struct bt_gatt_ccc_managed_user_data *ccc)
{
    for (uint8_t i = 0U; i < rtbus_ble_app_gatt_count; i++) {
        struct rtbus_ble_app_gatt *gatt = &rtbus_ble_app_gatts[i];

        for (uint8_t j = 0U; j < gatt->chrc_count; j++) {
            if (ccc == &gatt->chrcs[j].value_ccc) {
                return &gatt->chrcs[j];
            }
        }
    }

    return NULL;
}

static void rtbus_ble_app_gatt_ccc_changed(const struct bt_gatt_attr *attr,
                                           uint16_t value)
{
    struct bt_gatt_ccc_managed_user_data *ccc = attr->user_data;
    struct rtbus_ble_app_gatt_chrc *chrc =
        rtbus_ble_app_gatt_find_chrc_ccc(ccc);

    if (chrc == NULL) {
        return;
    }

    chrc->notify_enabled = value == BT_GATT_CCC_NOTIFY;
    LOG_DBG("BLE app GATT CCC notify=%u",
            chrc->notify_enabled ? 1U : 0U);
}

static ssize_t rtbus_ble_app_gatt_ccc_cfg_write(struct bt_conn *conn,
                                                const struct bt_gatt_attr *attr,
                                                uint16_t value)
{
    struct bt_gatt_ccc_managed_user_data *ccc = attr->user_data;
    struct rtbus_ble_app_gatt_chrc *chrc;

    ARG_UNUSED(conn);

    chrc = rtbus_ble_app_gatt_find_chrc_ccc(ccc);

    if (chrc != NULL) {
        chrc->notify_enabled = value == BT_GATT_CCC_NOTIFY;
    }

    LOG_DBG("BLE app GATT CCC write handle=0x%04x value=0x%04x notify=%u",
            attr->handle,
            value,
            value == BT_GATT_CCC_NOTIFY ? 1U : 0U);
    return sizeof(value);
}

#if RTBUS_BLE_GATT_TRACE
static uint8_t rtbus_ble_app_gatt_dump_attr(const struct bt_gatt_attr *attr,
                                            uint16_t handle,
                                            void *user_data)
{
    char uuid[BT_UUID_STR_LEN];

    ARG_UNUSED(user_data);

    bt_uuid_to_str(attr->uuid, uuid, sizeof(uuid));
    LOG_DBG("BLE app GATT DB attr handle=0x%04x uuid=%s perm=0x%02x",
            handle, uuid, attr->perm);
    return BT_GATT_ITER_CONTINUE;
}

static void rtbus_ble_app_gatt_dump_db(const struct rtbus_ble_app_gatt *gatt)
{
    if (!gatt->registered) {
        return;
    }

    bt_gatt_foreach_attr(gatt->attrs[0].handle,
                         gatt->attrs[gatt->attr_count - 1U].handle,
                         rtbus_ble_app_gatt_dump_attr,
                         NULL);
}
#endif

static void rtbus_ble_app_gatt_cccd_define(struct rtbus_ble_app_gatt *gatt,
                                           struct rtbus_ble_app_gatt_chrc *chrc,
                                           uint8_t attr_index,
                                           uint8_t perm)
{
    chrc->value_ccc = (struct bt_gatt_ccc_managed_user_data)
        BT_GATT_CCC_MANAGED_USER_DATA_INIT(
            rtbus_ble_app_gatt_ccc_changed,
            rtbus_ble_app_gatt_ccc_cfg_write,
            NULL);

    gatt->attrs[attr_index].uuid = &rtbus_ble_uuid_gatt_ccc.uuid;
    gatt->attrs[attr_index].read = bt_gatt_attr_read_ccc;
    gatt->attrs[attr_index].write = bt_gatt_attr_write_ccc;
    gatt->attrs[attr_index].user_data = &chrc->value_ccc;
    gatt->attrs[attr_index].perm = perm;
    chrc->ccc_attr_index = attr_index;
    chrc->has_ccc = true;
}

static int rtbus_ble_app_gatt_value_copy(struct rtbus_ble_app_gatt_chrc *chrc,
                                         const void *value,
                                         uint8_t value_len)
{
    if (value_len > RUNTIME_BLE_GATT_VALUE_MAX) {
        return -EMSGSIZE;
    }

    if (value_len != 0U && value == NULL) {
        return -EINVAL;
    }

    if (value_len != 0U) {
        memcpy(chrc->value, value, value_len);
    }

    chrc->value_len = value_len;
    return 0;
}

static uint8_t rtbus_ble_app_gatt_properties(
    const struct runtime_ble_gatt_chrc_def *def)
{
    if (def->properties != 0U) {
        return def->properties;
    }

    return BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY;
}

static uint8_t rtbus_ble_app_gatt_value_perm(
    const struct runtime_ble_gatt_chrc_def *def)
{
    if (def->value_perm != 0U) {
        return def->value_perm;
    }

    return BT_GATT_PERM_READ;
}

static uint8_t rtbus_ble_app_gatt_ccc_perm(
    const struct runtime_ble_gatt_chrc_def *def)
{
    if (def->ccc_perm != 0U) {
        return def->ccc_perm;
    }

    return BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;
}

static int rtbus_ble_app_gatt_register_ready(
    struct rtbus_ble_app_gatt *gatt,
    uint8_t handle)
{
    int ret;

    if (gatt->registered) {
        return 0;
    }

    ret = bt_gatt_service_register(&gatt->service);
    if (ret != 0) {
        LOG_ERR("BLE app GATT register failed handle=%u ret=%d",
                (unsigned int)handle, ret);
        return ret;
    }

    gatt->registered = true;
    LOG_DBG("BLE app GATT registered handle=%u svc=0x%04x attrs=%u chrcs=%u",
            (unsigned int)handle,
            gatt->attrs[0].handle,
            (unsigned int)gatt->attr_count,
            (unsigned int)gatt->chrc_count);
#if RTBUS_BLE_GATT_TRACE
    rtbus_ble_app_gatt_dump_db(gatt);
#endif
    return 0;
}

static int rtbus_ble_app_gatt_register_pending(const char *phase)
{
    int ret;

    for (uint8_t i = 0U; i < rtbus_ble_app_gatt_count; i++) {
        if (!rtbus_ble_app_gatts[i].registered) {
            LOG_DBG("BLE app GATT register pending handle=%u phase=%s",
                    (unsigned int)i, phase);
        }

        ret = rtbus_ble_app_gatt_register_ready(&rtbus_ble_app_gatts[i], i);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static size_t rtbus_ble_ad_count(void)
{
    return rtbus_ble_ad_has_service_uuid ? ARRAY_SIZE(rtbus_ble_ad) : 1U;
}

static void rtbus_ble_ad_sync_service_uuid(void)
{
    if (rtbus_ble_app_gatt_count == 0U) {
        rtbus_ble_ad_has_service_uuid = false;
        memset(rtbus_ble_ad_service_uuid, 0, sizeof(rtbus_ble_ad_service_uuid));
        return;
    }

    memcpy(rtbus_ble_ad_service_uuid,
           rtbus_ble_app_gatts[0].service_uuid.val,
           sizeof(rtbus_ble_ad_service_uuid));
    rtbus_ble_ad_has_service_uuid = true;
}

static void rtbus_ble_app_gatt_reset(void)
{
    int ret;

    for (uint8_t i = 0U; i < rtbus_ble_app_gatt_count; i++) {
        struct rtbus_ble_app_gatt *gatt = &rtbus_ble_app_gatts[i];

        if (!gatt->registered) {
            continue;
        }

        ret = bt_gatt_service_unregister(&gatt->service);
        if (ret != 0) {
            LOG_ERR("BLE app GATT unregister failed handle=%u ret=%d",
                    (unsigned int)i, ret);
        }
    }

    rtbus_ble_app_gatt_count = 0U;
    memset(rtbus_ble_app_gatts, 0, sizeof(rtbus_ble_app_gatts));
    rtbus_ble_ad_sync_service_uuid();
}

static void rtbus_ble_reset_app_state(void)
{
    int ret;

    if (rtbus_ble_ready && rtbus_ble_advertising) {
        ret = bt_le_adv_stop();
        if (ret != 0) {
            LOG_ERR("BLE advertising stop failed during reset: %d", ret);
        }
    }

    rtbus_ble_advertising = false;
    rtbus_ble_current_conn = 0U;
    rtbus_ble_gap_security_level = 0U;
    rtbus_ble_gap_flags = 0U;
    rtbus_ble_app_ad_count = 0U;
    rtbus_ble_app_sd_count = 0U;
    memset(rtbus_ble_app_ad, 0, sizeof(rtbus_ble_app_ad));
    memset(rtbus_ble_app_sd, 0, sizeof(rtbus_ble_app_sd));
    memset(rtbus_ble_app_ad_values, 0, sizeof(rtbus_ble_app_ad_values));
    memset(rtbus_ble_app_sd_values, 0, sizeof(rtbus_ble_app_sd_values));
    rtbus_ble_app_gatt_reset();
}

static int rtbus_ble_app_gatt_notify_register_now(
    const struct runtime_ble_gatt_service_def *def)
{
    struct rtbus_ble_app_gatt *gatt;
    const struct runtime_ble_gatt_chrc_def *chrc_defs;
    uint8_t handle;
    uint8_t attr_index;
    int ret;

    if (def == NULL || def->chrc_count == 0U ||
        def->chrc_count > RTBUS_BLE_APP_GATT_CHRC_MAX ||
        def->chrcs_addr == 0U) {
        return -EINVAL;
    }

    if (rtbus_ble_app_gatt_count >= RTBUS_BLE_APP_GATT_MAX) {
        return -ENOMEM;
    }

    chrc_defs =
        (const struct runtime_ble_gatt_chrc_def *)(uintptr_t)def->chrcs_addr;
    handle = rtbus_ble_app_gatt_count;
    gatt = &rtbus_ble_app_gatts[handle];
    memset(gatt, 0, sizeof(*gatt));

    gatt->service_uuid.uuid.type = BT_UUID_TYPE_128;
    memcpy(gatt->service_uuid.val, def->service_uuid,
           sizeof(gatt->service_uuid.val));

    attr_index = 0U;

    gatt->attrs[attr_index].uuid = &rtbus_ble_uuid_gatt_primary.uuid;
    gatt->attrs[attr_index].read = bt_gatt_attr_read_service;
    gatt->attrs[attr_index].write = NULL;
    gatt->attrs[attr_index].user_data = &gatt->service_uuid.uuid;
    gatt->attrs[attr_index].perm = BT_GATT_PERM_READ;
    attr_index++;

    for (uint8_t i = 0U; i < def->chrc_count; i++) {
        const struct runtime_ble_gatt_chrc_def *chrc_def = &chrc_defs[i];
        struct rtbus_ble_app_gatt_chrc *chrc = &gatt->chrcs[i];
        uint8_t properties = rtbus_ble_app_gatt_properties(chrc_def);

        if ((uint8_t)(attr_index + 2U) > ARRAY_SIZE(gatt->attrs)) {
            memset(gatt, 0, sizeof(*gatt));
            return -ENOMEM;
        }

        chrc->value_uuid.uuid.type = BT_UUID_TYPE_128;
        memcpy(chrc->value_uuid.val, chrc_def->value_uuid,
               sizeof(chrc->value_uuid.val));

        ret = rtbus_ble_app_gatt_value_copy(
            chrc,
            (const void *)(uintptr_t)chrc_def->value_addr,
            chrc_def->value_len);
        if (ret != 0) {
            memset(gatt, 0, sizeof(*gatt));
            return ret;
        }

        chrc->value_chrc.uuid = &chrc->value_uuid.uuid;
        chrc->value_chrc.value_handle = 0U;
        chrc->value_chrc.properties = properties;

        gatt->attrs[attr_index].uuid = &rtbus_ble_uuid_gatt_chrc.uuid;
        gatt->attrs[attr_index].read = bt_gatt_attr_read_chrc;
        gatt->attrs[attr_index].write = NULL;
        gatt->attrs[attr_index].user_data = &chrc->value_chrc;
        gatt->attrs[attr_index].perm = BT_GATT_PERM_READ;
        attr_index++;

        chrc->value_attr_index = attr_index;
        gatt->attrs[attr_index].uuid = &chrc->value_uuid.uuid;
        gatt->attrs[attr_index].read = rtbus_ble_app_gatt_read;
        gatt->attrs[attr_index].write = NULL;
        gatt->attrs[attr_index].user_data = chrc;
        gatt->attrs[attr_index].perm =
            rtbus_ble_app_gatt_value_perm(chrc_def);
        attr_index++;

        if ((properties & BT_GATT_CHRC_NOTIFY) != 0U) {
            if (attr_index >= ARRAY_SIZE(gatt->attrs)) {
                memset(gatt, 0, sizeof(*gatt));
                return -ENOMEM;
            }

            rtbus_ble_app_gatt_cccd_define(
                gatt,
                chrc,
                attr_index,
                rtbus_ble_app_gatt_ccc_perm(chrc_def));
            attr_index++;
        }
    }

    gatt->chrc_count = def->chrc_count;
    gatt->attr_count = attr_index;
    gatt->service.attrs = gatt->attrs;
    gatt->service.attr_count = gatt->attr_count;
    gatt->prepared = true;
    rtbus_ble_app_gatt_count++;
    rtbus_ble_ad_sync_service_uuid();

    if (rtbus_ble_ready) {
        ret = rtbus_ble_app_gatt_register_ready(gatt, handle);
        if (ret != 0) {
            return ret;
        }
    }

    LOG_DBG("BLE app GATT staged handle=%u attrs=%u chrcs=%u",
            (unsigned int)handle,
            (unsigned int)gatt->attr_count,
            (unsigned int)gatt->chrc_count);
    return handle;
}

static int rtbus_ble_app_gatt_value_set_now(
    const struct application_ble_gatt_value_set_req *req)
{
    struct rtbus_ble_app_gatt *gatt;
    struct rtbus_ble_app_gatt_chrc *chrc;
    struct bt_conn *conn = NULL;
    bool subscribed = false;
    int ret;

    if (req == NULL || req->handle >= rtbus_ble_app_gatt_count) {
        return -EINVAL;
    }

    gatt = &rtbus_ble_app_gatts[req->handle];
    if (!gatt->prepared || req->chrc >= gatt->chrc_count) {
        return -EINVAL;
    }

    chrc = &gatt->chrcs[req->chrc];

    ret = rtbus_ble_app_gatt_value_copy(
        chrc,
        (const void *)(uintptr_t)req->value_addr,
        req->value_len);
    if (ret != 0) {
        return ret;
    }

    if (rtbus_ble_current_conn != 0U) {
        conn = (struct bt_conn *)(uintptr_t)rtbus_ble_current_conn;
    }

    if (gatt->registered && chrc->has_ccc && conn != NULL) {
        subscribed = bt_gatt_is_subscribed(conn,
                                           &gatt->attrs[chrc->value_attr_index],
                                           BT_GATT_CCC_NOTIFY);
        chrc->notify_enabled = subscribed;
    }

    LOG_DBG("BLE app GATT value set handle=%u chrc=%u len=%u cached_notify=%u subscribed=%u conn=0x%08x",
            (unsigned int)req->handle,
            (unsigned int)req->chrc,
            (unsigned int)chrc->value_len,
            chrc->notify_enabled ? 1U : 0U,
            subscribed ? 1U : 0U,
            rtbus_ble_current_conn);

    if (gatt->registered && chrc->has_ccc && subscribed && conn != NULL) {
        ret = bt_gatt_notify(conn, &gatt->attrs[chrc->value_attr_index],
                             chrc->value, chrc->value_len);
        if (ret != 0) {
            LOG_ERR("BLE app GATT notify failed handle=%u chrc=%u ret=%d",
                    (unsigned int)req->handle, (unsigned int)req->chrc, ret);
            return ret;
        }
    }

    return 0;
}

static int rtbus_ble_gap_data_copy(
    struct bt_data *dst,
    uint8_t dst_values[][RUNTIME_BLE_GAP_DATA_VALUE_MAX],
    uint8_t *dst_count,
    uint8_t count,
    uint32_t defs_addr)
{
    const struct runtime_ble_gap_data_def *defs;

    if (count > RUNTIME_BLE_GAP_DATA_MAX) {
        return -ENOMEM;
    }

    if (count != 0U && defs_addr == 0U) {
        return -EINVAL;
    }

    *dst_count = 0U;
    memset(dst, 0, sizeof(struct bt_data) * RUNTIME_BLE_GAP_DATA_MAX);
    memset(dst_values, 0,
           RUNTIME_BLE_GAP_DATA_MAX * RUNTIME_BLE_GAP_DATA_VALUE_MAX);

    if (count == 0U) {
        return 0;
    }

    defs = (const struct runtime_ble_gap_data_def *)(uintptr_t)defs_addr;
    for (uint8_t i = 0U; i < count; i++) {
        if (defs[i].data_len > RUNTIME_BLE_GAP_DATA_VALUE_MAX) {
            return -EMSGSIZE;
        }

        memcpy(dst_values[i], defs[i].data, defs[i].data_len);
        dst[i].type = defs[i].type;
        dst[i].data_len = defs[i].data_len;
        dst[i].data = dst_values[i];
    }

    *dst_count = count;
    return 0;
}

static int rtbus_ble_gap_init_now(const struct runtime_ble_gap_def *def)
{
    int ret;

    if (def == NULL) {
        return -EINVAL;
    }

    ret = rtbus_ble_gap_data_copy(rtbus_ble_app_ad,
                                  rtbus_ble_app_ad_values,
                                  &rtbus_ble_app_ad_count,
                                  def->adv_data_count,
                                  def->adv_data_addr);
    if (ret != 0) {
        return ret;
    }

    ret = rtbus_ble_gap_data_copy(rtbus_ble_app_sd,
                                  rtbus_ble_app_sd_values,
                                  &rtbus_ble_app_sd_count,
                                  def->scan_data_count,
                                  def->scan_data_addr);
    if (ret != 0) {
        return ret;
    }

    rtbus_ble_gap_security_level = def->security_level;
    rtbus_ble_gap_flags = def->flags;
    LOG_DBG("BLE GAP initialized security_level=%u flags=0x%02x adv=%u scan=%u",
            (unsigned int)rtbus_ble_gap_security_level,
            (unsigned int)rtbus_ble_gap_flags,
            (unsigned int)rtbus_ble_app_ad_count,
            (unsigned int)rtbus_ble_app_sd_count);
    return 0;
}

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
#if RTBUS_BLE_GATT_TRACE
    for (uint8_t i = 0U; i < rtbus_ble_app_gatt_count; i++) {
        rtbus_ble_app_gatt_dump_db(&rtbus_ble_app_gatts[i]);
    }
#endif

    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_CONNECTED,
                                    payload,
                                    sizeof(payload));

    if (rtbus_ble_gap_security_level != 0U) {
        int ret = bt_conn_set_security(
            conn,
            (bt_security_t)rtbus_ble_gap_security_level);
        if (ret != 0) {
            LOG_ERR("BLE GAP auto security request failed: %d", ret);
        } else {
            LOG_DBG("BLE GAP auto security requested level=%u",
                    (unsigned int)rtbus_ble_gap_security_level);
        }
    }
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
static bool rtbus_ble_security_err_stale_bond(enum bt_security_err err)
{
    return err == BT_SECURITY_ERR_AUTH_FAIL ||
           err == BT_SECURITY_ERR_PIN_OR_KEY_MISSING ||
           err == BT_SECURITY_ERR_AUTH_REQUIREMENT ||
           err == BT_SECURITY_ERR_KEY_REJECTED;
}

static void rtbus_ble_forget_stale_bond(struct bt_conn *conn,
                                        enum bt_security_err err)
{
    const bt_addr_le_t *peer;
    int ret;

    if (!rtbus_ble_security_err_stale_bond(err)) {
        return;
    }

    peer = bt_conn_get_dst(conn);
    if (peer == NULL) {
        return;
    }

    ret = bt_unpair(BT_ID_DEFAULT, peer);
    if (ret != 0 && ret != -ENOENT) {
        LOG_ERR("BLE stale bond delete failed: err=%u ret=%d",
                (unsigned int)err, ret);
        return;
    }

    LOG_DBG("BLE stale bond deleted after security err=%u",
            (unsigned int)err);
}

static void rtbus_ble_security_changed(struct bt_conn *conn,
                                       bt_security_t level,
                                       enum bt_security_err err)
{
    uint8_t payload[2];

    payload[0] = (uint8_t)level;
    payload[1] = (uint8_t)err;
    (void)native_service_emit_event(RUNTIME_EVENT_GROUP_BLE,
                                    RUNTIME_EVENT_BLE_SECURITY_CHANGED,
                                    payload,
                                    sizeof(payload));

    if (err != BT_SECURITY_ERR_SUCCESS) {
        LOG_ERR("BLE security failed: level %u err %u",
                (unsigned int)level, (unsigned int)err);
        rtbus_ble_forget_stale_bond(conn, err);
        return;
    }

    LOG_INF("BLE security enabled: level %u", (unsigned int)level);
}

static void rtbus_ble_pairing_complete(struct bt_conn *conn, bool bonded)
{
    ARG_UNUSED(conn);

    LOG_DBG("BLE pairing complete bonded=%u", bonded ? 1U : 0U);
}

static void rtbus_ble_pairing_failed(struct bt_conn *conn,
                                     enum bt_security_err reason)
{
    LOG_ERR("BLE pairing failed: reason %u", (unsigned int)reason);
    rtbus_ble_forget_stale_bond(conn, reason);
}

static void rtbus_ble_bond_deleted(uint8_t id, const bt_addr_le_t *peer)
{
    ARG_UNUSED(peer);

    LOG_DBG("BLE bond deleted id=%u", (unsigned int)id);
}

static struct bt_conn_auth_info_cb rtbus_ble_auth_info_callbacks = {
    .pairing_complete = rtbus_ble_pairing_complete,
    .pairing_failed = rtbus_ble_pairing_failed,
    .bond_deleted = rtbus_ble_bond_deleted,
};

static int rtbus_ble_register_auth_info_callbacks(void)
{
    int ret;

    ret = bt_conn_auth_info_cb_register(&rtbus_ble_auth_info_callbacks);
    if (ret == -EALREADY) {
        return 0;
    }

    if (ret != 0) {
        LOG_ERR("BLE auth info callback register failed: %d", ret);
        return ret;
    }

    return 0;
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

    LOG_DBG("BLE security requested: level %u", (unsigned int)level);
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
        ret = rtbus_ble_app_gatt_register_pending("pre-enable");
        if (ret != 0) {
            return ret;
        }

        ret = bt_enable(NULL);
        if (ret != 0) {
            LOG_ERR("BLE enable failed: %d", ret);
            return ret;
        }

#if defined(CONFIG_BT_SMP)
        ret = rtbus_ble_register_auth_info_callbacks();
        if (ret != 0) {
            return ret;
        }
#endif

#if defined(CONFIG_SETTINGS)
        ret = settings_load();
        if (ret != 0) {
            LOG_ERR("BLE settings load failed: %d", ret);
            return ret;
        }

        LOG_DBG("BLE settings loaded");
#endif

        rtbus_ble_ready = true;
    }

    /* Covers app GATT services registered after the Bluetooth stack is ready. */
    ret = rtbus_ble_app_gatt_register_pending("ready");
    if (ret != 0) {
        return ret;
    }

    if (rtbus_ble_app_gatt_count == 0U) {
        LOG_WRN("BLE advertising without app GATT services");
    }

    rtbus_ble_ad_sync_service_uuid();

    if (rtbus_ble_advertising) {
        return 0;
    }

    if (rtbus_ble_app_ad_count != 0U || rtbus_ble_app_sd_count != 0U) {
        ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                              rtbus_ble_app_ad,
                              rtbus_ble_app_ad_count,
                              rtbus_ble_app_sd,
                              rtbus_ble_app_sd_count);
    } else {
        ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                              rtbus_ble_ad,
                              rtbus_ble_ad_count(),
                              rtbus_ble_sd,
                              ARRAY_SIZE(rtbus_ble_sd));
    }
    if (ret != 0) {
        LOG_ERR("BLE advertising start failed: %d", ret);
        return ret;
    }

    rtbus_ble_advertising = true;
    LOG_DBG("BLE advertising started");
    return 0;
}

static uint8_t rtbus_ble_process(rtbus_ctx_t *ctx)
{
    const struct application_ble_pair_req *pair_req;
    const struct application_ble_set_security_req *security_req;
    const struct application_ble_gap_init_req *gap_init_req;
    const struct application_ble_gatt_notify_register_req *gatt_register_req;
    const struct application_ble_gatt_value_set_req *gatt_value_set_req;
    int ret;

    switch (ctx->id) {
        case RTBUS_SYS_EVT(RTBUS_SYS_INIT):
            rtbus_ble_reset_app_state();
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

        case APPLICATION_BLE_GAP_INIT:
            if (ctx->payload_len != APPLICATION_BLE_GAP_INIT_PAYLOAD_SIZE) {
                break;
            }

            gap_init_req =
                (const struct application_ble_gap_init_req *)ctx->payload;
            ret = rtbus_ble_gap_init_now(
                (const struct runtime_ble_gap_def *)(uintptr_t)
                    gap_init_req->def_addr);
            if (native_service_complete_result(gap_init_req->result_addr,
                                               ret) != 0) {
                LOG_ERR("BLE GAP init result write failed: 0x%08x",
                        gap_init_req->result_addr);
            }
            break;

        case APPLICATION_BLE_GATT_NOTIFY_REGISTER:
            if (ctx->payload_len !=
                APPLICATION_BLE_GATT_NOTIFY_REGISTER_PAYLOAD_SIZE) {
                break;
            }

            gatt_register_req =
                (const struct application_ble_gatt_notify_register_req *)
                    ctx->payload;
            ret = rtbus_ble_app_gatt_notify_register_now(
                (const struct runtime_ble_gatt_service_def *)(uintptr_t)
                    gatt_register_req->def_addr);
            if (native_service_complete_result(gatt_register_req->result_addr,
                                               ret) != 0) {
                LOG_ERR("BLE GATT register result write failed: 0x%08x",
                        gatt_register_req->result_addr);
            }
            break;

        case APPLICATION_BLE_GATT_VALUE_SET:
            if (ctx->payload_len != APPLICATION_BLE_GATT_VALUE_SET_PAYLOAD_SIZE) {
                break;
            }

            gatt_value_set_req =
                (const struct application_ble_gatt_value_set_req *)ctx->payload;
            ret = rtbus_ble_app_gatt_value_set_now(gatt_value_set_req);
            if (native_service_complete_result(gatt_value_set_req->result_addr,
                                               ret) != 0) {
                LOG_ERR("BLE GATT value result write failed: 0x%08x",
                        gatt_value_set_req->result_addr);
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
