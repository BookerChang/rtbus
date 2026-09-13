/*
 * SPDX-License-Identifier: MPL-2.0
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

typedef int (*rtbus_post_api_t)(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len);
typedef size_t (*rtbus_serial_write_api_t)(uint32_t port, const uint8_t *data, size_t size);
typedef int32_t (*rtbus_serial_read_api_t)(uint32_t port);
typedef int (*rtbus_gpio_configure_api_t)(uint32_t pin, uint32_t mode);
typedef int (*rtbus_gpio_write_api_t)(uint32_t pin, uint32_t value);
typedef int (*rtbus_gpio_read_api_t)(uint32_t pin);
typedef void (*runtime_event_callback_t)( const struct runtime_event_tlv *event);
typedef int (*rtbus_on_event_api_t)(runtime_event_callback_t callback);
typedef uint32_t (*rtbus_millis_api_t)(void);

extern int rtbus_api_is_ready(void);
extern int rtbus_on_event(runtime_event_callback_t callback);
extern int rtbus_post_wait_result(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len, volatile int32_t *result);
extern size_t rtbus_serial_write(uint32_t port, const uint8_t *data, size_t size);
extern size_t rtbus_serial_vprintf(uint32_t port, const char *fmt, va_list args);
extern int32_t rtbus_serial_read(uint32_t port);
extern int rtbus_gpio_configure(uint32_t pin, uint32_t mode);
extern int rtbus_gpio_write(uint32_t pin, uint32_t value);
extern int rtbus_gpio_read(uint32_t pin);
extern int rtbus_post(uint8_t task_id, uint32_t ctx_id, const void *payload, uint32_t payload_len);
extern int32_t k_delay(uint32_t delay_ms);
extern uint32_t rtbus_millis(void);
extern void rtbus_rtt_vprintf(const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_API_CORE_H_ */
