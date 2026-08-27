# SPDX-License-Identifier: MPL-2.0
#
# Standalone native application build path kept with the Zephyr apps.
# This preserves the legacy native compile/link/pack flow while
# Arduino CLI remains available for sketch-based setup()/loop() builds.

BOARD_PROFILE ?= rak4631
TARGET ?= application
MODULE ?= application
MODULE_NAME ?= application

APPLICATION_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ZEPHYR_APPS_DIR := $(abspath $(APPLICATION_DIR)/..)
REPO_ROOT := $(abspath $(ZEPHYR_APPS_DIR)/../..)
SYSTEM_DIR := $(REPO_ROOT)/system
PLATFORM_DIR := $(REPO_ROOT)

BUILD_DIR ?= $(REPO_ROOT)/build.rtbus.application.$(BOARD_PROFILE)
SRC ?= $(APPLICATION_DIR)/main.c
STARTUP_SRC ?= $(SYSTEM_DIR)/rtduo_startup.S
LD_SCRIPT ?= $(APPLICATION_DIR)/boards/$(BOARD_PROFILE)/linker.ld
ABI_INCLUDE ?= $(SYSTEM_DIR)/include
PACK_SRC ?= $(SYSTEM_DIR)/header.c
APPLICATION_VERSION ?= 0.1.0
APPLICATION_BUILD ?= 0

PREFIX ?= arm-zephyr-eabi-
ifdef GCC_PATH
TOOLCHAIN_PREFIX := $(GCC_PATH)/$(PREFIX)
else
TOOLCHAIN_PREFIX := $(PREFIX)
endif

CC := $(TOOLCHAIN_PREFIX)gcc
LD := $(TOOLCHAIN_PREFIX)ld
CP := $(TOOLCHAIN_PREFIX)objcopy
SZ := $(TOOLCHAIN_PREFIX)size
HOST_CC ?= cc

include $(APPLICATION_DIR)/boards/$(BOARD_PROFILE)/board.mk
CPU ?= -mcpu=$(TARGET_MCU)
FPU ?=
FLOAT_ABI ?= -mabi=aapcs
MCU := $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

OPT ?= -Os
C_DEFS ?=
C_INCLUDES ?= -I$(ABI_INCLUDE)

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -Wextra \
	-ffreestanding -fdata-sections -ffunction-sections -fno-common
LDFLAGS += -T $(LD_SCRIPT) -Map $(BUILD_DIR)/$(TARGET).map --gc-sections
LDFLAGS += -L $(REPO_ROOT)

PACKER := $(BUILD_DIR)/$(TARGET)_pack
OBJECT := $(BUILD_DIR)/$(TARGET).o
STARTUP_OBJECT := $(BUILD_DIR)/startup.o
OBJECTS := $(STARTUP_OBJECT) $(OBJECT)
ELF := $(BUILD_DIR)/$(TARGET).elf
MAP := $(BUILD_DIR)/$(TARGET).map
BIN := $(BUILD_DIR)/$(TARGET).bin
PAYLOAD_BIN := $(BUILD_DIR)/$(MODULE).payload.bin
SIGNED_BIN := $(BUILD_DIR)/$(MODULE).signed.bin
SIGNED_HEX := $(BUILD_DIR)/$(MODULE).signed.hex
SLOT_TXT := $(BUILD_DIR)/slot_address.txt

.PHONY: all compile clean

all: compile

compile: $(SIGNED_HEX)
	@$(SZ) $(ELF)

$(BUILD_DIR):
	@mkdir -p $@

$(PACKER): $(PACK_SRC) | $(BUILD_DIR)
	$(HOST_CC) -x c -std=c99 -Wall -Wextra -Werror -O2 -o $@ $<

$(OBJECT): $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(STARTUP_OBJECT): $(STARTUP_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ELF): $(OBJECTS) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(BIN): $(ELF)
	$(CP) -O binary $< $@

$(SIGNED_HEX): $(BIN) $(ELF) $(PACKER)
	$(PACKER) true $(TARGET) $(BIN) $(PAYLOAD_BIN) $(MAP) $(SIGNED_BIN) $(SIGNED_HEX) $(SLOT_TXT) $(MODULE_NAME) $(APPLICATION_VERSION) $(APPLICATION_BUILD)

clean:
	rm -rf $(BUILD_DIR)
