/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_RUNTIME_SERIAL_H_
#define RTBUS_RUNTIME_SERIAL_H_

#include <stddef.h>
#include <stdint.h>

int rtbus_runtime_serial_begin(uint32_t port, uint32_t baud);
int rtbus_runtime_serial_flush_rx(uint32_t port);
size_t rtbus_runtime_serial_read(uint32_t port, uint8_t *data, size_t size);
size_t rtbus_runtime_serial_write(uint32_t port, const uint8_t *data,
                                  size_t size);

#endif /* RTBUS_RUNTIME_SERIAL_H_ */
