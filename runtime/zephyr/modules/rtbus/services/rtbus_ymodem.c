/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rtbus_image.h>
#include <rtbus/runtime_serial.h>
#include <rtbus/ymodem.h>

LOG_MODULE_REGISTER(rtbus_ymodem, LOG_LEVEL_INF);

#define YMODEM_SOH 0x01
#define YMODEM_STX 0x02
#define YMODEM_EOT 0x04
#define YMODEM_ACK 0x06
#define YMODEM_NAK 0x15
#define YMODEM_CAN 0x18
#define YMODEM_CRC 'C'

#define YMODEM_BLOCK_128 128U
#define YMODEM_BLOCK_1024 1024U
#define YMODEM_PACKET_OVERHEAD 4U
#define YMODEM_START_TIMEOUT_MS 30000
#define YMODEM_BYTE_TIMEOUT_MS 3000
#define YMODEM_READY_INTERVAL_MS 1000

static uint8_t ymodem_packet[YMODEM_BLOCK_1024 + YMODEM_PACKET_OVERHEAD];
static uint8_t ymodem_data[YMODEM_BLOCK_1024];

static uint16_t ymodem_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0U;

    for (size_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void ymodem_put(uint32_t port, uint8_t byte)
{
    (void)rtbus_runtime_serial_write(port, &byte, 1U);
}

static int ymodem_read_byte(uint32_t port, uint8_t *byte, int32_t timeout_ms)
{
    int64_t deadline = k_uptime_get() + timeout_ms;

    while (k_uptime_get() < deadline) {
        if (rtbus_runtime_serial_read(port, byte, 1U) == 1U) {
            return 0;
        }

        k_sleep(K_MSEC(1));
    }

    return -ETIMEDOUT;
}

static int ymodem_read_exact(uint32_t port, uint8_t *data, size_t len,
                             int32_t timeout_ms)
{
    for (size_t i = 0U; i < len; i++) {
        int ret = ymodem_read_byte(port, &data[i], timeout_ms);

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int ymodem_wait_header(uint32_t port, uint8_t *header)
{
    int64_t deadline = k_uptime_get() + YMODEM_START_TIMEOUT_MS;
    int64_t next_ready = 0;

    while (k_uptime_get() < deadline) {
        if (k_uptime_get() >= next_ready) {
            ymodem_put(port, YMODEM_CRC);
            next_ready = k_uptime_get() + YMODEM_READY_INTERVAL_MS;
        }

        if (rtbus_runtime_serial_read(port, header, 1U) == 1U) {
            return 0;
        }

        k_sleep(K_MSEC(1));
    }

    return -ETIMEDOUT;
}

static int ymodem_read_packet(uint32_t port, uint8_t header, uint8_t *block_no,
                              uint8_t *data, size_t *data_len)
{
    uint16_t expected_crc;
    uint16_t actual_crc;
    size_t payload_len;
    int ret;

    if (header == YMODEM_SOH) {
        payload_len = YMODEM_BLOCK_128;
    } else if (header == YMODEM_STX) {
        payload_len = YMODEM_BLOCK_1024;
    } else {
        return -EINVAL;
    }

    ret = ymodem_read_exact(port, ymodem_packet,
                            payload_len + YMODEM_PACKET_OVERHEAD,
                            YMODEM_BYTE_TIMEOUT_MS);
    if (ret != 0) {
        return ret;
    }

    if ((uint8_t)(ymodem_packet[0] + ymodem_packet[1]) != 0xFFU) {
        return -EBADMSG;
    }

    expected_crc = ((uint16_t)ymodem_packet[payload_len + 2U] << 8) |
                   ymodem_packet[payload_len + 3U];
    actual_crc = ymodem_crc16(&ymodem_packet[2], payload_len);
    if (actual_crc != expected_crc) {
        return -EBADMSG;
    }

    *block_no = ymodem_packet[0];
    *data_len = payload_len;
    memcpy(data, &ymodem_packet[2], payload_len);

    return 0;
}

static size_t ymodem_parse_file_size(const uint8_t *block, size_t len)
{
    size_t pos = 0U;

    while (pos < len && block[pos] != '\0') {
        pos++;
    }

    if (pos >= len || (pos + 1U) >= len || block[pos + 1U] == '\0') {
        return 0U;
    }

    return (size_t)strtoul((const char *)&block[pos + 1U], NULL, 10);
}

static void ymodem_log_file_name(const uint8_t *block, size_t len)
{
    char name[64];
    size_t i;

    for (i = 0U; i < (sizeof(name) - 1U) && i < len && block[i] != '\0'; i++) {
        name[i] = (char)block[i];
    }
    name[i] = '\0';

    LOG_INF("YMODEM file: %s", name);
}

int rtbus_ymodem_recv(uint32_t port)
{
    struct rtbus_image_store_info store_info;
    size_t data_len;
    size_t expected_size = 0U;
    size_t received_size = 0U;
    size_t write_len;
    uint8_t expected_block = 1U;
    bool got_metadata = false;
    bool store_active = false;
    bool eot_seen = false;
    int ret;

    ret = rtbus_runtime_serial_begin(port, 115200U);
    if (ret != 0) {
        return ret;
    }

    (void)rtbus_runtime_serial_flush_rx(port);
    LOG_INF("YMODEM receive test start");

    while (true) {
        uint8_t header;
        uint8_t block_no;

        if (!got_metadata) {
            ret = ymodem_wait_header(port, &header);
        } else {
            ret = ymodem_read_byte(port, &header, YMODEM_BYTE_TIMEOUT_MS);
        }
        if (ret != 0) {
            LOG_ERR("YMODEM header receive failed: %d", ret);
            if (store_active) {
                (void)rtbus_image_store_finish(ret);
            }
            return ret;
        }

        if (header == YMODEM_CAN) {
            LOG_ERR("YMODEM canceled by sender");
            if (store_active) {
                (void)rtbus_image_store_finish(-ECANCELED);
            }
            return -ECANCELED;
        }

        if (header == YMODEM_EOT) {
            if (!eot_seen) {
                eot_seen = true;
                ymodem_put(port, YMODEM_NAK);
            } else {
                ymodem_put(port, YMODEM_ACK);
                ymodem_put(port, YMODEM_CRC);
            }
            continue;
        }

        ret = ymodem_read_packet(port, header, &block_no, ymodem_data,
                                 &data_len);
        if (ret != 0) {
            LOG_WRN("YMODEM packet receive failed: %d", ret);
            ymodem_put(port, YMODEM_NAK);
            continue;
        }

        if (!got_metadata) {
            if (block_no != 0U) {
                ymodem_put(port, YMODEM_NAK);
                continue;
            }

            got_metadata = true;
            expected_size = ymodem_parse_file_size(ymodem_data, data_len);
            ymodem_log_file_name(ymodem_data, data_len);
            LOG_INF("YMODEM expected size: %u", (unsigned int)expected_size);

            if (expected_size == 0U) {
                ymodem_put(port, YMODEM_NAK);
                return -EINVAL;
            }

            ymodem_put(port, YMODEM_ACK);
            ymodem_put(port, YMODEM_CRC);

            store_info.image_size = expected_size;
            ret = rtbus_image_store_begin(&store_info);
            if (ret != 0) {
                LOG_ERR("RTBus image store begin failed: %d", ret);
                ymodem_put(port, YMODEM_CAN);
                return ret;
            }

            store_active = true;
            continue;
        }

        if (eot_seen && block_no == 0U) {
            ret = rtbus_image_store_finish(0);
            store_active = false;
            if (ret != 0) {
                LOG_ERR("RTBus image store finish failed: %d", ret);
                ymodem_put(port, YMODEM_CAN);
                return ret;
            }

            ymodem_put(port, YMODEM_ACK);
            LOG_INF("YMODEM receive test done: received=%u expected=%u",
                    (unsigned int)received_size,
                    (unsigned int)expected_size);
            return 0;
        }

        if (block_no != expected_block) {
            LOG_WRN("YMODEM unexpected block: got=%u expected=%u",
                    block_no, expected_block);
            ymodem_put(port, YMODEM_NAK);
            continue;
        }

        write_len = data_len;
        if (write_len > (expected_size - received_size)) {
            write_len = expected_size - received_size;
        }

        ret = rtbus_image_store_write(received_size, ymodem_data, write_len);
        if (ret != 0) {
            LOG_ERR("RTBus image store write failed: %d", ret);
            (void)rtbus_image_store_finish(ret);
            ymodem_put(port, YMODEM_CAN);
            return ret;
        }

        received_size += write_len;
        expected_block++;
        ymodem_put(port, YMODEM_ACK);
    }
}
