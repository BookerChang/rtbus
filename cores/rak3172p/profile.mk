# SPDX-License-Identifier: MPL-2.0

PROFILE_NAME := rak3172p
ZEPHYR_BOARD ?= rak3172
ZEPHYR_BOARD_SOURCE ?= upstream
ZEPHYR_SOC ?= stm32wle5cc
JLINK_TARGET ?= STM32WLE5CC
RUNTIME_BOARD_DIR ?= cores/rak3172p/zephyr
BOOTLOADER_BOARD_DIR ?= cores/rak3172p/zephyr
BOARD_RUNTIME_CONF ?= cores/rak3172p/zephyr/runtime.conf
BOARD_RUNTIME_OVERLAY ?= cores/rak3172p/zephyr/runtime.overlay
BOARD_BOOTLOADER_CONF ?= cores/rak3172p/zephyr/bootloader.conf
BOARD_BOOTLOADER_OVERLAY ?= cores/rak3172p/zephyr/bootloader.overlay
