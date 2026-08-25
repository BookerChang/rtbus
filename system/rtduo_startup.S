/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal RTDuo application startup.
 *
 * This is not an MCU reset/vector startup. The Zephyr RTBus runtime has
 * already created the native application thread before entering here.
 */

.syntax unified
.thumb

#include "runtime_api_slots.h"

.equ WISNODEZ_API_SLOT_RTBUS_POST_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_RTBUS_POST)
.equ WISNODEZ_API_SLOT_DELAY_OFFSET,      WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_DELAY)
.equ WISNODEZ_API_SLOT_PRINTK_OFFSET,     WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_PRINTK)
.equ WISNODEZ_API_SLOT_VPRINTK_OFFSET,    WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_VPRINTK)
.equ WISNODEZ_API_SLOT_RESULT_WAIT_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_RESULT_WAIT)
.equ WISNODEZ_API_SLOT_SERIAL_WRITE_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_SERIAL_WRITE)
.equ WISNODEZ_API_SLOT_GPIO_CONFIGURE_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_GPIO_CONFIGURE)
.equ WISNODEZ_API_SLOT_GPIO_WRITE_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_GPIO_WRITE)
.equ WISNODEZ_API_SLOT_GPIO_READ_OFFSET,  WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_GPIO_READ)
.equ WISNODEZ_API_SLOT_ON_EVENT_OFFSET,   WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_ON_EVENT)
.equ WISNODEZ_API_SLOT_MAGIC_OFFSET,      WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_MAGIC)
.equ WISNODEZ_API_SLOT_VERSION_OFFSET,    WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_VERSION)
.equ WISNODEZ_API_SLOT_SLOT_COUNT_OFFSET, WISNODEZ_API_SLOT_OFFSET(WISNODEZ_API_SLOT_SLOT_COUNT)

.section .data.__rtduo_api_table_base, "aw", %progbits
.align 2
.global __rtduo_api_table_base
.type __rtduo_api_table_base, %object
__rtduo_api_table_base:
#if defined(CONFIG_WISNODEZ_API_TABLE_BASE)
    .word CONFIG_WISNODEZ_API_TABLE_BASE
#else
    .word __wisnodez_api_table_base
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

RTDUO_API_THUNK rtbus_post, WISNODEZ_API_SLOT_RTBUS_POST_OFFSET
RTDUO_API_THUNK k_delay,    WISNODEZ_API_SLOT_DELAY_OFFSET
RTDUO_API_THUNK printk,     WISNODEZ_API_SLOT_PRINTK_OFFSET
RTDUO_API_THUNK vprintk,    WISNODEZ_API_SLOT_VPRINTK_OFFSET
RTDUO_API_THUNK runtime_serial_write,   WISNODEZ_API_SLOT_SERIAL_WRITE_OFFSET
RTDUO_API_THUNK runtime_gpio_configure, WISNODEZ_API_SLOT_GPIO_CONFIGURE_OFFSET
RTDUO_API_THUNK runtime_gpio_write,     WISNODEZ_API_SLOT_GPIO_WRITE_OFFSET
RTDUO_API_THUNK runtime_gpio_read,      WISNODEZ_API_SLOT_GPIO_READ_OFFSET

.section .text.runtime_api_is_ready, "ax", %progbits
.align 2
.global runtime_api_is_ready
.type runtime_api_is_ready, %function
.thumb_func
runtime_api_is_ready:
    ldr r0, =__rtduo_api_table_base
    ldr r0, [r0]

    ldr r1, [r0, #WISNODEZ_API_SLOT_MAGIC_OFFSET]
    ldr r2, =WZ_API_MAGIC
    cmp r1, r2
    bne 1f

    ldr r1, [r0, #WISNODEZ_API_SLOT_VERSION_OFFSET]
    cmp r1, #WZ_API_VERSION
    bne 1f

    ldr r1, [r0, #WISNODEZ_API_SLOT_SLOT_COUNT_OFFSET]
    cmp r1, #WISNODEZ_API_SLOT_COUNT
    blo 1f

    movs r0, #1
    bx lr

1:
    movs r0, #0
    bx lr
.size runtime_api_is_ready, . - runtime_api_is_ready

.section .text.runtime_on_event, "ax", %progbits
.align 2
.global runtime_on_event
.type runtime_on_event, %function
.thumb_func
runtime_on_event:
    ldr r12, =__rtduo_api_table_base
    ldr r12, [r12]
    ldr r12, [r12, #WISNODEZ_API_SLOT_ON_EVENT_OFFSET]
    cmp r12, #0
    beq 1f
    bx r12

1:
    movs r0, #WZ_ENOSYS
    rsbs r0, r0, #0
    bx lr
.size runtime_on_event, . - runtime_on_event

.section .text.runtime_post_wait_result, "ax", %progbits
.align 2
.global runtime_post_wait_result
.type runtime_post_wait_result, %function
.thumb_func
runtime_post_wait_result:
    push {r4, r5, r6, lr}
    ldr r4, [sp, #16]
    movs r5, #WZ_EINPROGRESS
    rsbs r5, r5, #0
    str r5, [r4]

    ldr r5, =__rtduo_api_table_base
    ldr r5, [r5]
    ldr r6, [r5, #WISNODEZ_API_SLOT_RESULT_WAIT_OFFSET]
    ldr r12, [r5, #WISNODEZ_API_SLOT_RTBUS_POST_OFFSET]
    blx r12
    cmp r0, #0
    bne 1f

    ldr r0, =WZ_WAIT_FOREVER
    blx r6
    cmp r0, #0
    bne 1f

    ldr r0, [r4]

1:
    pop {r4, r5, r6, pc}
.size runtime_post_wait_result, . - runtime_post_wait_result
