/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <rtbus.h>
#include <rtbus/cli.h>
#include <rtbus/native_service.h>
#include <rtbus/runtime_serial.h>

#include "runtime_abi.h"

LOG_MODULE_REGISTER(rtbus_serial, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(rtbus_application_serial)
#error "rtbus,application-serial is required by native Serial API"
#endif

#define RTBUS_SERIAL_NODE DT_CHOSEN(rtbus_application_serial)
#define RTBUS_SERIAL_RX_BUFFER_SIZE 2048U
#define RTBUS_SERIAL_DEV_OR_NULL(_chosen) \
    COND_CODE_1(DT_HAS_CHOSEN(_chosen), \
                (DEVICE_DT_GET(DT_CHOSEN(_chosen))), \
                (NULL))
#define RTBUS_SERIAL_CHOSEN_IS_CDC(_chosen) \
    COND_CODE_1(DT_HAS_CHOSEN(_chosen), \
                (DT_NODE_HAS_COMPAT(DT_CHOSEN(_chosen), zephyr_cdc_acm_uart)), \
                (0))

struct rtbus_runtime_serial_port {
    const struct device *dev;
    struct k_spinlock lock;
    size_t rx_head;
    size_t rx_tail;
    bool is_cdc;
    bool rx_ready;
    bool prepared;
    uint8_t rx_buffer[RTBUS_SERIAL_RX_BUFFER_SIZE];
};

static char rtbus_runtime_serial_control_line[RTBUS_CLI_LINE_MAX];
static size_t rtbus_runtime_serial_control_line_len;
static bool rtbus_runtime_serial_control_active;
static bool rtbus_runtime_serial_control_overflow;
static bool rtbus_runtime_serial_port0_line_start = true;

static struct rtbus_runtime_serial_port rtbus_runtime_serial_ports[] = {
    [RTBUS_SERIAL_PORT_0] = {
        .dev = DEVICE_DT_GET(RTBUS_SERIAL_NODE),
        .is_cdc = RTBUS_SERIAL_CHOSEN_IS_CDC(rtbus_application_serial),
    },
    [RTBUS_SERIAL_PORT_1] = {
        .dev = RTBUS_SERIAL_DEV_OR_NULL(rtbus_application_serial1),
        .is_cdc = RTBUS_SERIAL_CHOSEN_IS_CDC(rtbus_application_serial1),
    },
};

static struct rtbus_runtime_serial_port *rtbus_runtime_serial_port(uint32_t port)
{
    if (port >= ARRAY_SIZE(rtbus_runtime_serial_ports)) {
        return NULL;
    }

    return &rtbus_runtime_serial_ports[port];
}

static void rtbus_runtime_serial_rx_put(struct rtbus_runtime_serial_port *port,
                                        uint8_t byte)
{
    k_spinlock_key_t key;
    size_t next_head;

    key = k_spin_lock(&port->lock);

    next_head = (port->rx_head + 1U) % sizeof(port->rx_buffer);
    if (next_head == port->rx_tail) {
        port->rx_tail = (port->rx_tail + 1U) % sizeof(port->rx_buffer);
    }

    port->rx_buffer[port->rx_head] = byte;
    port->rx_head = next_head;

    k_spin_unlock(&port->lock, key);
}

static void rtbus_runtime_serial_rx_clear(struct rtbus_runtime_serial_port *port)
{
    k_spinlock_key_t key;

    key = k_spin_lock(&port->lock);
    port->rx_head = 0U;
    port->rx_tail = 0U;
    k_spin_unlock(&port->lock, key);
}

static bool rtbus_runtime_serial_rx_get(struct rtbus_runtime_serial_port *port,
                                        uint8_t *byte)
{
    k_spinlock_key_t key;
    bool has_data = false;

    key = k_spin_lock(&port->lock);

    if (port->rx_tail != port->rx_head) {
        *byte = port->rx_buffer[port->rx_tail];
        port->rx_tail = (port->rx_tail + 1U) % sizeof(port->rx_buffer);
        has_data = true;
    }

    k_spin_unlock(&port->lock, key);
    return has_data;
}

static void rtbus_runtime_serial_submit_control_line(void)
{
    if (!rtbus_runtime_serial_control_overflow) {
        for (size_t i = 0U; i < rtbus_runtime_serial_control_line_len; i++) {
            rtbus_cli_submit_byte(rtbus_runtime_serial_control_line[i]);
        }

        rtbus_cli_submit_byte('\n');
    }

    rtbus_runtime_serial_control_line_len = 0U;
    rtbus_runtime_serial_control_active = false;
    rtbus_runtime_serial_control_overflow = false;
    rtbus_runtime_serial_port0_line_start = true;
}

static bool rtbus_runtime_serial_route_port0_byte(uint8_t byte)
{
    if (rtbus_runtime_serial_control_active) {
        if (byte == '\r' || byte == '\n') {
            rtbus_runtime_serial_submit_control_line();
            return false;
        }

        if (rtbus_runtime_serial_control_line_len <
            (sizeof(rtbus_runtime_serial_control_line) - 1U)) {
            rtbus_runtime_serial_control_line[
                rtbus_runtime_serial_control_line_len++] = (char)byte;
        } else {
            rtbus_runtime_serial_control_overflow = true;
        }

        return false;
    }

    if (rtbus_runtime_serial_port0_line_start && byte == '@') {
        rtbus_runtime_serial_control_active = true;
        rtbus_runtime_serial_port0_line_start = false;
        rtbus_runtime_serial_control_line_len = 0U;
        rtbus_runtime_serial_control_overflow = false;
        rtbus_runtime_serial_control_line[
            rtbus_runtime_serial_control_line_len++] = (char)byte;
        return false;
    }

    rtbus_runtime_serial_port0_line_start =
        (byte == '\r' || byte == '\n');

    return true;
}

static void rtbus_runtime_serial_receive_byte(
    struct rtbus_runtime_serial_port *port, uint8_t byte)
{
    if (port == &rtbus_runtime_serial_ports[RTBUS_SERIAL_PORT_0] &&
        !rtbus_runtime_serial_route_port0_byte(byte)) {
        return;
    }

    rtbus_runtime_serial_rx_put(port, byte);
}

static void rtbus_runtime_serial_drain_fifo(
    struct rtbus_runtime_serial_port *port)
{
    uint8_t buffer[16];

    while (true) {
        int received = uart_fifo_read(port->dev, buffer, sizeof(buffer));

        if (received <= 0) {
            break;
        }

        for (int i = 0; i < received; i++) {
            rtbus_runtime_serial_receive_byte(port, buffer[i]);
        }
    }
}

static void rtbus_runtime_serial_irq_callback(const struct device *dev,
                                              void *user_data)
{
    struct rtbus_runtime_serial_port *port = user_data;

    ARG_UNUSED(dev);

    while (uart_irq_update(port->dev) != 0 && uart_irq_rx_ready(port->dev)) {
        rtbus_runtime_serial_drain_fifo(port);
    }
}

static void rtbus_runtime_serial_cdc_callback(const struct device *dev,
                                              void *user_data)
{
    struct rtbus_runtime_serial_port *port = user_data;

    ARG_UNUSED(dev);

    rtbus_runtime_serial_drain_fifo(port);
}

static int rtbus_runtime_serial_prepare_rx(
    struct rtbus_runtime_serial_port *serial_port)
{
    int ret;

    if (serial_port->rx_ready) {
        return 0;
    }

    if (serial_port->dev == NULL || !device_is_ready(serial_port->dev)) {
        return -ENODEV;
    }

    ret = uart_irq_callback_user_data_set(serial_port->dev,
                                          serial_port->is_cdc ?
                                          rtbus_runtime_serial_cdc_callback :
                                          rtbus_runtime_serial_irq_callback,
                                          serial_port);
    if (ret != 0) {
        return ret;
    }

    uart_irq_rx_enable(serial_port->dev);
    serial_port->rx_ready = true;

    return 0;
}

static int rtbus_runtime_serial_configure(
    struct rtbus_runtime_serial_port *serial_port, uint32_t baud)
{
    struct uart_config config;
    bool rx_was_enabled;
    int ret;

    if (baud == 0U) {
        return -EINVAL;
    }

    if (serial_port->dev == NULL || !device_is_ready(serial_port->dev)) {
        return -ENODEV;
    }

    ret = uart_config_get(serial_port->dev, &config);
    if (ret != 0) {
        config.baudrate = baud;
        config.parity = UART_CFG_PARITY_NONE;
        config.stop_bits = UART_CFG_STOP_BITS_1;
        config.data_bits = UART_CFG_DATA_BITS_8;
        config.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
    } else {
        config.baudrate = baud;
    }

    rx_was_enabled = !serial_port->is_cdc && serial_port->rx_ready;
    if (rx_was_enabled) {
        uart_irq_rx_disable(serial_port->dev);
    }

    ret = uart_configure(serial_port->dev, &config);

    if (rx_was_enabled) {
        uart_irq_rx_enable(serial_port->dev);
    }

    if (ret == -ENOSYS || ret == -ENOTSUP) {
        return serial_port->is_cdc ? 0 : ret;
    }

    return ret;
}

int rtbus_runtime_serial_begin(uint32_t port, uint32_t baud)
{
    struct rtbus_runtime_serial_port *serial_port =
        rtbus_runtime_serial_port(port);
    int ret;

    if (serial_port == NULL) {
        return -EINVAL;
    }

    ret = rtbus_runtime_serial_configure(serial_port, baud);
    if (ret != 0) {
        return ret;
    }

    if (serial_port->prepared) {
        return 0;
    }

    ret = rtbus_runtime_serial_prepare_rx(serial_port);
    if (ret != 0) {
        return ret;
    }

    serial_port->prepared = true;

    return 0;
}

int rtbus_runtime_serial_flush_rx(uint32_t port)
{
    struct rtbus_runtime_serial_port *serial_port =
        rtbus_runtime_serial_port(port);

    if (serial_port == NULL) {
        return -EINVAL;
    }

    rtbus_runtime_serial_rx_clear(serial_port);

    return 0;
}

static bool rtbus_runtime_serial_tx_ready(
    const struct rtbus_runtime_serial_port *serial_port)
{
    uint32_t dtr = 0U;

    if (!serial_port->is_cdc) {
        return true;
    }

    return uart_line_ctrl_get(serial_port->dev, UART_LINE_CTRL_DTR, &dtr) == 0 &&
           dtr != 0U;
}

size_t rtbus_runtime_serial_write(uint32_t port, const uint8_t *data,
                                  size_t size)
{
    struct rtbus_runtime_serial_port *serial_port =
        rtbus_runtime_serial_port(port);
    size_t written = 0U;

    if (serial_port == NULL || serial_port->dev == NULL ||
        !serial_port->prepared || data == NULL || size == 0U) {
        return 0U;
    }

    if (!device_is_ready(serial_port->dev)) {
        return 0U;
    }

    if (!rtbus_runtime_serial_tx_ready(serial_port)) {
        return 0U;
    }

    for (size_t i = 0U; i < size; i++) {
        uart_poll_out(serial_port->dev, data[i]);
        written++;
    }

    return written;
}

size_t rtbus_runtime_serial_read(uint32_t port, uint8_t *data, size_t size)
{
    struct rtbus_runtime_serial_port *serial_port =
        rtbus_runtime_serial_port(port);
    size_t received = 0U;

    if (serial_port == NULL || serial_port->dev == NULL ||
        !serial_port->prepared || data == NULL || size == 0U) {
        return 0U;
    }

    if (!device_is_ready(serial_port->dev)) {
        return 0U;
    }

    while (received < size) {
        unsigned char byte;

        if (rtbus_runtime_serial_rx_get(serial_port, &byte)) {
            data[received++] = byte;
            continue;
        }

        if (serial_port->is_cdc ||
            serial_port->rx_ready ||
            uart_poll_in(serial_port->dev, &byte) != 0) {
            break;
        }

        data[received++] = byte;
    }

    return received;
}

static int rtbus_runtime_serial_start_port(uint32_t port)
{
    struct rtbus_runtime_serial_port *serial_port =
        rtbus_runtime_serial_port(port);
    int ret;

    if (serial_port == NULL) {
        return -EINVAL;
    }

    if (serial_port->dev == NULL) {
        return 0;
    }

    ret = rtbus_runtime_serial_prepare_rx(serial_port);

    if (ret != 0) {
        LOG_ERR("runtime serial port %u init failed: %d", port, ret);
        return ret;
    }

    serial_port->prepared = true;
    return 0;
}

static int rtbus_runtime_serial_start(void)
{
    int ret;

    for (uint32_t port = 0U;
         port < ARRAY_SIZE(rtbus_runtime_serial_ports);
         port++) {
        ret = rtbus_runtime_serial_start_port(port);
        if (ret != 0) {
            LOG_ERR("runtime serial port %u start skipped: %d",
                    port, ret);
        }
    }

    return 0;
}

static uint8_t rtbus_runtime_serial_process(rtbus_ctx_t *ctx)
{
    int ret;

    switch (ctx->id) {
        case RTBUS_SYS_EVT(RTBUS_SYS_INIT):
            ret = rtbus_runtime_serial_start();
            if (ret != 0) {
                LOG_ERR("runtime serial start failed: %d", ret);
            }
            break;

        case APPLICATION_RUNTIME_SERIAL_BEGIN:
            {
                struct application_runtime_serial_begin_req req;

                if (ctx->payload_len != sizeof(req)) {
                    LOG_ERR("serial begin payload size mismatch: %u",
                            ctx->payload_len);
                    return 0;
                }

                memcpy(&req, ctx->payload, sizeof(req));
                ret = rtbus_runtime_serial_begin(req.port, req.baud);
                ret = native_service_complete_result(req.result_addr, ret);
                if (ret != 0) {
                    LOG_ERR("serial begin result completion failed: %d", ret);
                    return 0;
                }
            }
            break;

        default:
            break;
    }

    return 0;
}

RTBUS_TASK_REGISTER(rtbus_runtime_serial_task,
                    RTBUS_TASK_RUNTIME_SERIAL,
                    RTBUS_TASK_PRIORITY_LOW,
                    rtbus_runtime_serial_process);
