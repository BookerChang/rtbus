/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include "native_board.h"

#define RTBUS_NATIVE_GPIO_NODE DT_NODELABEL(rtbus_native_gpio)

#if DT_NODE_EXISTS(RTBUS_NATIVE_GPIO_NODE) && \
    DT_NODE_HAS_PROP(RTBUS_NATIVE_GPIO_NODE, gpios)
static const struct gpio_dt_spec native_gpio_pins[] = {
    DT_FOREACH_PROP_ELEM_SEP(RTBUS_NATIVE_GPIO_NODE,
                             gpios,
                             GPIO_DT_SPEC_GET_BY_IDX,
                             (,))
};
#endif

const struct gpio_dt_spec *native_board_gpio_pins(size_t *count)
{
#if DT_NODE_EXISTS(RTBUS_NATIVE_GPIO_NODE) && \
    DT_NODE_HAS_PROP(RTBUS_NATIVE_GPIO_NODE, gpios)
    *count = ARRAY_SIZE(native_gpio_pins);
    return native_gpio_pins;
#else
    *count = 0U;
    return NULL;
#endif
}
