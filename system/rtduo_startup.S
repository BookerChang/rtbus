/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * Minimal RTDuo application startup.
 *
 * This is not an MCU reset/vector startup. The Zephyr RTBus runtime has
 * already created the native application thread before entering here.
 */

.syntax unified
.thumb

#include "runtime_api_slots.h"

.equ RTBUS_API_SLOT_RTBUS_POST_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_RTBUS_POST)
.equ RTBUS_API_SLOT_DELAY_OFFSET,      RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_DELAY)
.equ RTBUS_API_SLOT_RTT_VPRINTF_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_RTT_VPRINTF)
.equ RTBUS_API_SLOT_RESULT_WAIT_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_RESULT_WAIT)
.equ RTBUS_API_SLOT_SERIAL_WRITE_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_SERIAL_WRITE)
.equ RTBUS_API_SLOT_GPIO_CONFIGURE_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_GPIO_CONFIGURE)
.equ RTBUS_API_SLOT_GPIO_WRITE_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_GPIO_WRITE)
.equ RTBUS_API_SLOT_GPIO_READ_OFFSET,  RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_GPIO_READ)
.equ RTBUS_API_SLOT_ON_EVENT_OFFSET,   RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_ON_EVENT)
.equ RTBUS_API_SLOT_SERIAL_VPRINTF_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_SERIAL_VPRINTF)
.equ RTBUS_API_SLOT_MAGIC_OFFSET,      RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_MAGIC)
.equ RTBUS_API_SLOT_VERSION_OFFSET,    RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_VERSION)
.equ RTBUS_API_SLOT_SLOT_COUNT_OFFSET, RTBUS_API_SLOT_OFFSET(RTBUS_API_SLOT_SLOT_COUNT)

.section .data.__rtduo_api_table_base, "aw", %progbits
.align 2
.global __rtduo_api_table_base
.type __rtduo_api_table_base, %object
__rtduo_api_table_base:
#if defined(CONFIG_RTBUS_NATIVE_API_TABLE_BASE)
    .word CONFIG_RTBUS_NATIVE_API_TABLE_BASE
#else
    .word __rtbus_native_api_table_base
#endif
.size __rtduo_api_table_base, . - __rtduo_api_table_base

.section .text.__rtduo_start, "ax", %progbits
.align 2
.global __rtduo_start
.type __rtduo_start, %function
.thumb_func
__rtduo_start:
    b main
.size __rtduo_start, . - __rtduo_start

.macro RTDUO_API_THUNK name, offset
.section .text.\name, "ax", %progbits
.align 2
.global \name
.type \name, %function
.thumb_func
\name:
    ldr r12, =__rtduo_api_table_base
    ldr r12, [r12]
    ldr r12, [r12, #\offset]
    bx r12
.size \name, . - \name
.endm

RTDUO_API_THUNK rtbus_post, RTBUS_API_SLOT_RTBUS_POST_OFFSET
RTDUO_API_THUNK k_delay,    RTBUS_API_SLOT_DELAY_OFFSET
RTDUO_API_THUNK rtbus_rtt_vprintf, RTBUS_API_SLOT_RTT_VPRINTF_OFFSET
RTDUO_API_THUNK rtbus_serial_write,   RTBUS_API_SLOT_SERIAL_WRITE_OFFSET
RTDUO_API_THUNK rtbus_gpio_configure, RTBUS_API_SLOT_GPIO_CONFIGURE_OFFSET
RTDUO_API_THUNK rtbus_gpio_write,     RTBUS_API_SLOT_GPIO_WRITE_OFFSET
RTDUO_API_THUNK rtbus_gpio_read,      RTBUS_API_SLOT_GPIO_READ_OFFSET
RTDUO_API_THUNK rtbus_serial_vprintf, RTBUS_API_SLOT_SERIAL_VPRINTF_OFFSET

.section .text.rtbus_api_is_ready, "ax", %progbits
.align 2
.global rtbus_api_is_ready
.type rtbus_api_is_ready, %function
.thumb_func
rtbus_api_is_ready:
    ldr r0, =__rtduo_api_table_base
    ldr r0, [r0]

    ldr r1, [r0, #RTBUS_API_SLOT_MAGIC_OFFSET]
    ldr r2, =RTBUS_API_MAGIC
    cmp r1, r2
    bne 1f

    ldr r1, [r0, #RTBUS_API_SLOT_VERSION_OFFSET]
    cmp r1, #RTBUS_API_VERSION
    bne 1f

    ldr r1, [r0, #RTBUS_API_SLOT_SLOT_COUNT_OFFSET]
    cmp r1, #RTBUS_API_SLOT_COUNT
    blo 1f

    movs r0, #1
    bx lr

1:
    movs r0, #0
    bx lr
.size rtbus_api_is_ready, . - rtbus_api_is_ready

.section .text.rtbus_on_event, "ax", %progbits
.align 2
.global rtbus_on_event
.type rtbus_on_event, %function
.thumb_func
rtbus_on_event:
    ldr r12, =__rtduo_api_table_base
    ldr r12, [r12]
    ldr r12, [r12, #RTBUS_API_SLOT_ON_EVENT_OFFSET]
    cmp r12, #0
    beq 1f
    bx r12

1:
    movs r0, #RTBUS_ENOSYS
    rsbs r0, r0, #0
    bx lr
.size rtbus_on_event, . - rtbus_on_event

.section .text.rtbus_post_wait_result, "ax", %progbits
.align 2
.global rtbus_post_wait_result
.type rtbus_post_wait_result, %function
.thumb_func
rtbus_post_wait_result:
    push {r4, r5, r6, lr}
    ldr r4, [sp, #16]
    movs r5, #RTBUS_EINPROGRESS
    rsbs r5, r5, #0
    str r5, [r4]

    ldr r5, =__rtduo_api_table_base
    ldr r5, [r5]
    ldr r6, [r5, #RTBUS_API_SLOT_RESULT_WAIT_OFFSET]
    ldr r12, [r5, #RTBUS_API_SLOT_RTBUS_POST_OFFSET]
    blx r12
    cmp r0, #0
    bne 1f

    ldr r0, =RTBUS_WAIT_FOREVER
    blx r6
    cmp r0, #0
    bne 1f

    ldr r0, [r4]

1:
    pop {r4, r5, r6, pc}
.size rtbus_post_wait_result, . - rtbus_post_wait_result
