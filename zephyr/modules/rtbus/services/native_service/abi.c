/*
 * SPDX-License-Identifier: MPL-2.0
 */

static void native_api_table_install(void)
{
    volatile uintptr_t *api_table =
        (volatile uintptr_t *)(uintptr_t)CONFIG_RTBUS_NATIVE_API_TABLE_BASE;

    api_table[RTBUS_API_SLOT_MAGIC] =
        RTBUS_API_MAGIC;
    api_table[RTBUS_API_SLOT_VERSION] =
        RTBUS_API_VERSION;
    api_table[RTBUS_API_SLOT_SLOT_COUNT] =
        RTBUS_API_SLOT_COUNT;
    api_table[RTBUS_API_SLOT_RTBUS_POST] =
        (uintptr_t)native_api_rtbus_post;
    api_table[RTBUS_API_SLOT_DELAY] =
        (uintptr_t)native_api_k_delay;
    api_table[RTBUS_API_SLOT_RTT_VPRINTF] =
        (uintptr_t)native_api_rtt_vprintf;
    api_table[RTBUS_API_SLOT_RESULT_WAIT] =
        (uintptr_t)native_api_result_wait;
    api_table[RTBUS_API_SLOT_SERIAL_WRITE] =
        (uintptr_t)native_api_serial_write;
    api_table[RTBUS_API_SLOT_GPIO_CONFIGURE] =
        (uintptr_t)native_api_gpio_configure;
    api_table[RTBUS_API_SLOT_GPIO_WRITE] =
        (uintptr_t)native_api_gpio_write;
    api_table[RTBUS_API_SLOT_GPIO_READ] =
        (uintptr_t)native_api_gpio_read;
    api_table[RTBUS_API_SLOT_ON_EVENT] =
        (uintptr_t)native_api_on_event;
    api_table[RTBUS_API_SLOT_SERIAL_VPRINTF] =
        (uintptr_t)native_api_serial_vprintf;
    api_table[RTBUS_API_SLOT_SERIAL_READ] =
        (uintptr_t)native_api_serial_read;
    api_table[RTBUS_API_SLOT_MILLIS] =
        (uintptr_t)native_api_millis;
}
