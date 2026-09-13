/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include "native_board.h"

#define RTBUS_NATIVE_GPIO_NODE DT_NODELABEL(rtbus_native_gpio)

#define NATIVE_BOARD_GPIO_PIN_ENTRY(node_id) \
    {                                        \
        .app_pin = DT_PROP(node_id, rtbus_pin), \
        .gpio = GPIO_DT_SPEC_GET(node_id, gpios), \
    }

#if DT_NODE_EXISTS(RTBUS_NATIVE_GPIO_NODE)
#if DT_CHILD_NUM(RTBUS_NATIVE_GPIO_NODE) > 0
static const struct native_board_gpio_pin native_gpio_pins[] = {
    DT_FOREACH_CHILD_SEP(RTBUS_NATIVE_GPIO_NODE,
                         NATIVE_BOARD_GPIO_PIN_ENTRY,
                         (,))
};
#endif
#endif

const struct native_board_gpio_pin *native_board_gpio_pins(size_t *count)
{
#if DT_NODE_EXISTS(RTBUS_NATIVE_GPIO_NODE)
#if DT_CHILD_NUM(RTBUS_NATIVE_GPIO_NODE) > 0
    *count = ARRAY_SIZE(native_gpio_pins);
    return native_gpio_pins;
#else
    *count = 0U;
    return NULL;
#endif
#else
    *count = 0U;
    return NULL;
#endif
}
