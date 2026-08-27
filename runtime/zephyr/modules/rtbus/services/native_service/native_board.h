/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H
#define RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H

#include <stddef.h>

#include <zephyr/drivers/gpio.h>

const struct gpio_dt_spec *native_board_gpio_pins(size_t *count);

#endif /* RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H */
