/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H
#define RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>

struct native_board_gpio_pin {
    uint32_t app_pin;
    struct gpio_dt_spec gpio;
};

const struct native_board_gpio_pin *native_board_gpio_pins(size_t *count);

#endif /* RTBUS_NATIVE_SERVICE_NATIVE_BOARD_H */
