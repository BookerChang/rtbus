# SPDX-License-Identifier: MPL-2.0
#
# Native application GCC build path shared by RTDuo cores.

BOARD_PROFILE ?= rak4631
TARGET ?= application
MODULE ?= application
MODULE_NAME ?= application

APPLICATION_MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(APPLICATION_MAKEFILE_DIR)
CORES_DIR := $(REPO_ROOT)/cores
SYSTEM_DIR := $(REPO_ROOT)/system
PROFILE_DIR := $(CORES_DIR)/$(BOARD_PROFILE)
PROFILE_MK := $(PROFILE_DIR)/profile.mk

ifeq ($(wildcard $(PROFILE_MK)),)
$(error Unsupported BOARD_PROFILE=$(BOARD_PROFILE))
endif

include $(PROFILE_MK)

BUILD_DIR ?= $(REPO_ROOT)/build.rtbus.application.$(BOARD_PROFILE)
SRC ?= $(PROFILE_DIR)/main.c
STARTUP_SRC ?= $(SYSTEM_DIR)/rtduo_startup.S
CORE_SOURCES ?= $(PROFILE_DIR)/wiring_time.c
LD_SCRIPT ?= $(PROFILE_DIR)/linker.ld
ABI_INCLUDE ?= $(SYSTEM_DIR)/include
PACK_SRC ?= $(SYSTEM_DIR)/header.c
APPLICATION_VERSION ?= 0.1.0
APPLICATION_BUILD ?= 0

TARGET_MCU ?= $(APPLICATION_TARGET_MCU)
ifeq ($(strip $(TARGET_MCU)),)
$(error APPLICATION_TARGET_MCU is required by $(PROFILE_MK))
endif

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
CORE_OBJECTS := $(patsubst $(PROFILE_DIR)/%.c,$(BUILD_DIR)/core_%.o,$(filter $(PROFILE_DIR)/%.c,$(CORE_SOURCES)))
OBJECTS := $(STARTUP_OBJECT) $(OBJECT) $(CORE_OBJECTS)
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

$(BUILD_DIR)/core_%.o: $(PROFILE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ELF): $(OBJECTS) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(BIN): $(ELF)
	$(CP) -O binary $< $@

$(SIGNED_HEX): $(BIN) $(ELF) $(PACKER)
	$(PACKER) true $(TARGET) $(BIN) $(PAYLOAD_BIN) $(MAP) $(SIGNED_BIN) $(SIGNED_HEX) $(SLOT_TXT) $(MODULE_NAME) $(APPLICATION_VERSION) $(APPLICATION_BUILD)

clean:
	rm -rf $(BUILD_DIR)
