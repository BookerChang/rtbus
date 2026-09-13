#pragma once

/*
 * RAK5005-O carrier-board Arduino pin identifiers.
 *
 * These values are application ABI pin numbers. The runtime owns the actual
 * board-to-Zephyr GPIO map and may translate each identifier differently per
 * module/carrier combination. STM32 GPIO IDs use bank-sized numbering:
 * PAx = IOx, PBx = IO(16 + x), PCx = IO(32 + x).
 */

#define IO0  0U  //BSP PA0
#define IO1  1U  //BSP PA1
#define IO2  2U
#define IO3  3U
#define IO4  4U
#define IO5  5U
#define IO6  6U
#define IO7  7U
#define IO8  8U
#define IO9  9U
#define IO10 10U
#define IO11 11U
#define IO12 12U
#define IO13 13U
#define IO14 14U
#define IO15 15U
#define IO16 16U
#define IO17 17U
#define IO18 18U
#define IO19 19U
#define IO20 20U
#define IO21 21U  //BSP PB5
#define IO22 22U
#define IO23 23U
#define IO24 24U
#define IO25 25U
#define IO26 26U
#define IO27 27U
#define IO28 28U
#define IO29 29U
#define IO30 30U
#define IO31 31U
#define IO32 32U
#define IO33 33U
#define IO34 34U
#define IO35 35U
#define IO36 36U
#define IO37 37U
#define IO38 38U
#define IO39 39U
#define IO40 40U
#define IO41 41U
#define IO42 42U
#define IO43 43U
#define IO44 44U
#define IO45 45U
#define IO46 46U
#define IO47 47U
