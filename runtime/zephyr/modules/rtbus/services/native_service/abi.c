/*
 * SPDX-License-Identifier: MPL-2.0
 */

static void native_abi_table_install(void)
{
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_MAGIC) =
        RTBUS_NATIVE_API_MAGIC;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_VERSION) =
        RTBUS_NATIVE_API_VERSION;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_SLOT_COUNT) =
        RTBUS_NATIVE_API_SLOT_COUNT;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_RTBUS_POST) =
        (uintptr_t)native_api_rtbus_post;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_DELAY) =
        (uintptr_t)native_api_k_delay;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_PRINTK) =
        (uintptr_t)native_api_printk;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_VPRINTK) =
        (uintptr_t)native_api_vprintk;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_RESULT_WAIT) =
        (uintptr_t)native_api_result_wait;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_SERIAL_WRITE) =
        (uintptr_t)native_api_serial_write;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_GPIO_CONFIGURE) =
        (uintptr_t)native_api_gpio_configure;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_GPIO_WRITE) =
        (uintptr_t)native_api_gpio_write;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_GPIO_READ) =
        (uintptr_t)native_api_gpio_read;
    RTBUS_NATIVE_API_SLOT_PTR(RTBUS_NATIVE_API_SLOT_ON_EVENT) =
        (uintptr_t)native_api_on_event;
}
