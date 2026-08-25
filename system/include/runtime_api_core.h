/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RUNTIME_API_CORE_H_
#define RUNTIME_API_CORE_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "runtime_api_slots.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTIME_EVENT_VALUE_MAX 256U

#include "runtime_abi.h"

struct runtime_event_tlv {
    uint16_t group;
    uint16_t tag;
    uint16_t len;
    uint16_t flags;
    uint8_t value[RUNTIME_EVENT_VALUE_MAX];
};

typedef int (*runtime_rtbus_post_api_t)(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len);
typedef int32_t (*runtime_delay_api_t)(uint32_t delay_ms);
typedef void (*runtime_printk_api_t)(const char *fmt, ...);
typedef void (*runtime_vprintk_api_t)(const char *fmt, va_list args);
typedef int (*runtime_result_wait_api_t)(uint32_t timeout_ms);
typedef size_t (*runtime_serial_write_api_t)(const uint8_t *data, size_t size);
typedef int (*runtime_gpio_configure_api_t)(uint32_t pin, uint32_t mode);
typedef int (*runtime_gpio_write_api_t)(uint32_t pin, uint32_t value);
typedef int (*runtime_gpio_read_api_t)(uint32_t pin);
typedef void (*runtime_event_callback_t)( const struct runtime_event_tlv *event);
typedef int (*runtime_on_event_api_t)(runtime_event_callback_t callback);

extern int runtime_api_is_ready(void);
extern int runtime_on_event(runtime_event_callback_t callback);
extern int runtime_post_wait_result(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len, volatile int32_t *result);
extern size_t runtime_serial_write(const uint8_t *data, size_t size);
extern int runtime_gpio_configure(uint32_t pin, uint32_t mode);
extern int runtime_gpio_write(uint32_t pin, uint32_t value);
extern int runtime_gpio_read(uint32_t pin);
extern int rtbus_post(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len);
extern int32_t k_delay(uint32_t delay_ms);
extern void printk(const char *fmt, ...);
extern void vprintk(const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_API_CORE_H_ */
