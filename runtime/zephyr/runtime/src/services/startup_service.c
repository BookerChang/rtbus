/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_USB_DEVICE_STACK_NEXT) && \
    !defined(CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT)
#include <zephyr/usb/usbd.h>
#endif

LOG_MODULE_REGISTER(startup_service, LOG_LEVEL_INF);

#define STARTUP_COMPONENT_NOR_FLASH_NODE DT_NODELABEL(component_nor_flash)
#define STARTUP_USB_VID 0x2FE3
#define STARTUP_USB_PID 0x0001
#define STARTUP_USB_DTR_POLL_INTERVAL_MS 100

#if defined(CONFIG_USB_DEVICE_STACK_NEXT) && \
    !defined(CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT)
USBD_DEVICE_DEFINE(startup_usb_cdc_acm,
           DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
           STARTUP_USB_VID, STARTUP_USB_PID);

USBD_DESC_LANG_DEFINE(startup_usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(startup_usb_mfr, "RAK");
USBD_DESC_PRODUCT_DEFINE(startup_usb_product, "RAK");
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(startup_usb_sn)));

USBD_DESC_CONFIG_DEFINE(startup_usb_fs_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(startup_usb_fs_config, 0, 125,
              &startup_usb_fs_desc);

static bool startup_usb_ready;
static bool startup_usb_dtr_ready;
static struct k_work_delayable startup_usb_dtr_work;

static int startup_usb_register_cdc_acm(struct usbd_context *const uds_ctx)
{
    int ret;

    ret = usbd_add_configuration(uds_ctx, USBD_SPEED_FS,
                     &startup_usb_fs_config);
    if (ret != 0) {
        LOG_ERR("USB add configuration failed: %d", ret);
        return ret;
    }

    ret = usbd_register_class(uds_ctx, "cdc_acm_0", USBD_SPEED_FS, 1);
    if (ret != 0) {
        LOG_ERR("USB register console CDC ACM failed: %d", ret);
        return ret;
    }

    ret = usbd_register_class(uds_ctx, "cdc_acm_1", USBD_SPEED_FS, 1);
    if (ret != 0) {
        LOG_ERR("USB register mcumgr CDC ACM failed: %d", ret);
        return ret;
    }

    return usbd_device_set_code_triple(uds_ctx, USBD_SPEED_FS,
                       USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

static void startup_usb_dtr_work_handler(struct k_work *work)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    uint32_t dtr = 0;

    ARG_UNUSED(work);

    if (!device_is_ready(uart_dev)) {
        k_work_reschedule(&startup_usb_dtr_work,
                  K_MSEC(STARTUP_USB_DTR_POLL_INTERVAL_MS));
        return;
    }

    if (uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr) != 0 ||
        dtr == 0U) {
        if (startup_usb_ready) {
            k_work_reschedule(&startup_usb_dtr_work,
                      K_MSEC(STARTUP_USB_DTR_POLL_INTERVAL_MS));
        }
        return;
    }

    if (!startup_usb_dtr_ready) {
        (void)uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DCD, 1);
        (void)uart_line_ctrl_set(uart_dev, UART_LINE_CTRL_DSR, 1);
        startup_usb_dtr_ready = true;
        LOG_INF("USB console DTR ready");
    }
}

static int startup_usb_init(void)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    int ret;

    if (startup_usb_ready) {
        return 0;
    }

    ret = usbd_add_descriptor(&startup_usb_cdc_acm, &startup_usb_lang);
    if (ret != 0) {
        LOG_ERR("USB language descriptor failed: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&startup_usb_cdc_acm, &startup_usb_mfr);
    if (ret != 0) {
        LOG_ERR("USB manufacturer descriptor failed: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&startup_usb_cdc_acm, &startup_usb_product);
    if (ret != 0) {
        LOG_ERR("USB product descriptor failed: %d", ret);
        return ret;
    }

    IF_ENABLED(CONFIG_HWINFO, (
        ret = usbd_add_descriptor(&startup_usb_cdc_acm, &startup_usb_sn);
    ))
    if (ret != 0) {
        LOG_ERR("USB serial descriptor failed: %d", ret);
        return ret;
    }

    ret = startup_usb_register_cdc_acm(&startup_usb_cdc_acm);
    if (ret != 0) {
        return ret;
    }

    ret = usbd_init(&startup_usb_cdc_acm);
    if (ret != 0) {
        LOG_ERR("USB device init failed: %d", ret);
        return ret;
    }

    ret = usbd_enable(&startup_usb_cdc_acm);
    if (ret != 0) {
        LOG_ERR("USB device enable failed: %d", ret);
        return ret;
    }

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("USB console UART not ready");
        return -ENODEV;
    }

    startup_usb_ready = true;
    k_work_init_delayable(&startup_usb_dtr_work,
                  startup_usb_dtr_work_handler);
    k_work_schedule(&startup_usb_dtr_work, K_NO_WAIT);

    LOG_INF("USB init done");

    return 0;
}
#endif

static int startup_service_init(void)
{
    int ret = 0;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT) && \
    !defined(CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT)
    ret = startup_usb_init();
    if (ret < 0) {
        LOG_ERR("USB initialization failed: %d", ret);
        return ret;
    }
#endif

#if DT_NODE_HAS_STATUS(STARTUP_COMPONENT_NOR_FLASH_NODE, okay)
    const struct device *flash_dev = DEVICE_DT_GET(STARTUP_COMPONENT_NOR_FLASH_NODE);

    if (!device_is_ready(flash_dev)) {
        LOG_ERR("component NOR flash is not ready");
        return -ENODEV;
    }

    LOG_INF("component NOR flash detected");
#endif

    return ret;
}

SYS_INIT(startup_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
