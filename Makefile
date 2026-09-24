PROJECT := rtbus

VM ?= podman
DOCKER_IMAGE ?= localhost/rtbus-zephyr:arm-1.0.0
DOCKER_WORK ?= /workdir
ARDUINO_CONFIG ?= arduino-cli.yaml
ARDUINO_PACKAGE_ROOT ?= build.arduino/package
ARDUINO_PACKAGE_DIR ?= $(ARDUINO_PACKAGE_ROOT)/hardware/rtbus/rtduo
ARDUINO_DIST_DIR ?= dist/arduino
ARDUINO_PACKAGE_INDEX ?= package_rtbus_index.json
ARDUINO_PACKAGE_BASE_URL ?=
ARDUINO_PACKAGE_TOOL_ARGS ?=
ARDUINO_SKETCH ?= libraries/RTDuo/examples/HelloWorld
ARDUINO_BUILD_ROOT ?= build.arduino
BOOTLOADER ?= bootloader
BOOTLOADER_APP_DIR ?= bootloader
RUNTIME_APP_DIR ?= runtime/zephyr/runtime
ZEPHYR_BUILD_ROOT ?= build.zephyr
ZEPHYR_BUILD_TMP_ROOT ?= build.zephyr.tmp
ZEPHYR_SHARE_ROOT ?= zephyr-share
APPLICATION_BACKEND ?= arduino
APPLICATION_GCC_MAKEFILE ?= application.mk
APPLICATION_SOURCE ?= cores/$(BOARD_PROFILE)/main.c
APPLICATION_GCC_PATH ?= /opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION)/gnu/arm-zephyr-eabi/bin
APPLICATION_VERSION ?= 0.1.0
APPLICATION_BUILD ?= 0

BOARD_PROFILE ?=
ARDUINO_BOARD_ID_rak4631 := RAK4631
ARDUINO_BOARD_ID_rak3172p := RAK3172P
ARDUINO_BOARD_ID_rak3172t := RAK3172T
ARDUINO_BOARD_ID_rak4200 := RAK4200
ARDUINO_BOARD_ID ?= $(ARDUINO_BOARD_ID_$(BOARD_PROFILE))
ARDUINO_FQBN ?= rtbus:rtduo:$(ARDUINO_BOARD_ID)
ARDUINO_APPLICATION_BUILD_DIR ?= $(ARDUINO_BUILD_ROOT)/$(BOARD_PROFILE)/application
ARDUINO_SKETCH_NAME ?= $(notdir $(ARDUINO_SKETCH))
ARDUINO_APPLICATION_JFLASH_HEX ?= $(ARDUINO_SKETCH_NAME).ino.signed.hex

ZEPHYR_SDK_VERSION ?= 1.0.0
ARDUINO_LOCAL_COMPILER_PATH ?= /opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION)/gnu/arm-zephyr-eabi/bin/
ARDUINO_LOCAL_HOST_COMPILER_PATH ?= /usr/bin/
ARDUINO_LOCAL_HOST_COMPILER_CMD ?= tcc
ARDUINO_LOCAL_HOST_COMPILER_FLAGS ?=
JLINK_CMD ?= myjlink
JLINK_SERVER_DIR ?= /home/usera/001.mypjt/002.server_segger
JLINK_EXE ?= $(JLINK_SERVER_DIR)/.res/JLink.linux/JLinkExe
JLINK_IP ?= 127.0.0.1:19020
JLINK_IF ?= SWD
JLINK_SPEED ?= 4000
JLINK_ERASE_SCRIPT ?= /tmp/rtbus-jlink-erase.jlink
RUNTIME_JFLASH_HEX ?= zephyr.signed.hex
BOOTLOADER_JFLASH_HEX ?= zephyr.hex
APPLICATION_JFLASH_HEX ?= application.signed.hex
APPLICATION_FLASH_DIR ?= $(if $(wildcard $(ARDUINO_APPLICATION_BUILD_DIR)/$(ARDUINO_APPLICATION_JFLASH_HEX)),$(ARDUINO_APPLICATION_BUILD_DIR),$(APPLICATION_BUILD_DIR))
APPLICATION_FLASH_HEX ?= $(if $(wildcard $(ARDUINO_APPLICATION_BUILD_DIR)/$(ARDUINO_APPLICATION_JFLASH_HEX)),$(ARDUINO_APPLICATION_JFLASH_HEX),$(APPLICATION_JFLASH_HEX))
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

include docker/docker.mk
include docker/github_ci.mk
include runtime/zephyr/profiles.mk
include select.mk

ifneq ($(strip $(BOARD_PROFILE)),)
BOARD_PROFILE_DIR := $(BOARD_PROFILE_ROOT)/$(BOARD_PROFILE)
BOARD_PROFILE_MK := $(BOARD_PROFILE_DIR)/profile.mk
ifeq ($(wildcard $(BOARD_PROFILE_MK)),)
$(error Unsupported BOARD_PROFILE=$(BOARD_PROFILE). Use one of: $(BOARD_PROFILE_CHOICES))
endif
include $(BOARD_PROFILE_MK)
endif
BOARDS ?= $(ZEPHYR_BOARD)
BOARD_ROOTS_DOCKER = $(foreach root,$(BOARD_ROOTS),$(DOCKER_WORK)/$(root))
BOARD_ROOT_CMAKE = $(subst $(SPACE),;,$(strip $(BOARD_ROOTS_DOCKER)))
BOARD_ROOT_ARG = $(if $(strip $(BOARD_ROOTS)),-DBOARD_ROOT="$(BOARD_ROOT_CMAKE)",)
docker_path = $(if $(filter /%,$(1)),$(1),$(DOCKER_WORK)/$(1))
APPLICATION_BUILD_DIR ?= $(ZEPHYR_BUILD_ROOT)/application/$(BOARD_PROFILE)
APPLICATION_BUILD_TMP_DIR ?= $(ZEPHYR_BUILD_TMP_ROOT)/application/$(BOARD_PROFILE)
APPLICATION_BUILD_DIR_DOCKER = $(call docker_path,$(APPLICATION_BUILD_DIR))
APPLICATION_BUILD_TMP_DIR_DOCKER = $(call docker_path,$(APPLICATION_BUILD_TMP_DIR))

include runtime/zephyr/runtime/runtime.mk
include runtime/zephyr/bootloader.mk

DOCKER_RUN = $(VM) run --rm \
	-v $(CURDIR):$(DOCKER_WORK) \
	-v $(RTBUS_ZEPHYR_VOLUME):$(RTBUS_ZEPHYR_WORKSPACE) \
	--tmpfs $(DOCKER_WORK)/.west \
	-w $(DOCKER_WORK) \
	-e RTBUS_ZEPHYR_WORKSPACE=$(RTBUS_ZEPHYR_WORKSPACE) \
	-e ZEPHYR_BASE=$(RTBUS_ZEPHYR_WORKSPACE)/zephyr \
	-e ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION) \
	-e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	$(DOCKER_IMAGE)
ARDUINO_LOCAL_BUILD_PROPERTIES = \
	--build-property compiler.path=$(ARDUINO_LOCAL_COMPILER_PATH) \
	--build-property compiler.host.path=$(ARDUINO_LOCAL_HOST_COMPILER_PATH) \
	--build-property compiler.host.cmd=$(ARDUINO_LOCAL_HOST_COMPILER_CMD) \
	--build-property compiler.host.flags=$(ARDUINO_LOCAL_HOST_COMPILER_FLAGS) \
	--build-property tools.ymodem_upload.cmd.path=$(ARDUINO_LOCAL_HOST_COMPILER_PATH)$(ARDUINO_LOCAL_HOST_COMPILER_CMD) \
	--build-property tools.ymodem_upload.flags=$(ARDUINO_LOCAL_HOST_COMPILER_FLAGS)

.DEFAULT_GOAL := arduino.boards

.PHONY: builder.image
builder.image: docker.image

.PHONY: $(BOARD_PROFILE_CHOICES)
$(BOARD_PROFILE_CHOICES):
	$(MAKE) runtime BOARD_PROFILE=$@

.PHONY: board.profile
board.profile:
	@printf 'BOARD_PROFILE=%s\n' '$(BOARD_PROFILE)'
	@printf 'BOARD_PROFILE_DIR=%s\n' '$(BOARD_PROFILE_DIR)'
	@printf 'PROFILE_NAME=%s\n' '$(PROFILE_NAME)'
	@printf 'BOARDS=%s\n' '$(BOARDS)'
	@printf 'ZEPHYR_BOARD=%s\n' '$(ZEPHYR_BOARD)'
	@printf 'ZEPHYR_BOARD_SOURCE=%s\n' '$(ZEPHYR_BOARD_SOURCE)'
	@printf 'ZEPHYR_SOC=%s\n' '$(ZEPHYR_SOC)'
	@printf 'BOARD_ROOTS=%s\n' '$(BOARD_ROOTS)'
	@printf 'BOARD_ROOT_CMAKE=%s\n' '$(BOARD_ROOT_CMAKE)'
	@printf 'BOARD_ROOT_ARG=%s\n' '$(BOARD_ROOT_ARG)'
	@printf 'JLINK_TARGET=%s\n' '$(JLINK_TARGET)'
	@printf 'JLINK_EXE=%s\n' '$(JLINK_EXE)'
	@printf 'JLINK_IP=%s\n' '$(JLINK_IP)'
	@printf 'RUNTIME_BOARD_DIR=%s\n' '$(RUNTIME_BOARD_DIR)'
	@printf 'BOARD_RUNTIME_CONF=%s\n' '$(BOARD_RUNTIME_CONF)'
	@printf 'BOARD_RUNTIME_OVERLAY=%s\n' '$(BOARD_RUNTIME_OVERLAY)'
	@printf 'BOOTLOADER_BOARD_DIR=%s\n' '$(BOOTLOADER_BOARD_DIR)'
	@printf 'BOARD_BOOTLOADER_CONF=%s\n' '$(BOARD_BOOTLOADER_CONF)'
	@printf 'BOARD_BOOTLOADER_OVERLAY=%s\n' '$(BOARD_BOOTLOADER_OVERLAY)'
	@printf 'APPLICATION_BACKEND=%s\n' '$(APPLICATION_BACKEND)'
	@printf 'APPLICATION_SOURCE=%s\n' '$(APPLICATION_SOURCE)'
	@printf 'APPLICATION_BUILD_DIR=%s\n' '$(APPLICATION_BUILD_DIR)'

.PHONY: jflash_erase
jflash_erase:
	$(JLINK_CMD).device $(JLINK_TARGET)
	@printf 'si 1\nspeed auto\nr\nh\nerase\nr\nq\n' > $(JLINK_ERASE_SCRIPT)
	$(JLINK_EXE) -device $(JLINK_TARGET) -if $(JLINK_IF) -speed $(JLINK_SPEED) -autoconnect 1 -nogui 1 -IP $(JLINK_IP) -CommanderScript $(JLINK_ERASE_SCRIPT)
	@rm -f $(JLINK_ERASE_SCRIPT)
	@echo "current time: $$(date +'%Y-%m-%d %H:%M:%S')"

.PHONY: jlink_rttlog
jlink_rttlog:
	$(JLINK_CMD).device $(JLINK_TARGET)
	myjlink.rttlog

.PHONY: jflash_write.runtime
jflash_write.runtime:
	$(JLINK_CMD).device $(JLINK_TARGET)
	@test -f "$(RUNTIME_BUILD_DIR)/zephyr/$(RUNTIME_JFLASH_HEX)" || { echo "Missing $(RUNTIME_BUILD_DIR)/zephyr/$(RUNTIME_JFLASH_HEX). Rebuild runtime and ensure MCUboot image generation is enabled." >&2; exit 1; }
	cd $(RUNTIME_BUILD_DIR)/zephyr && $(JLINK_CMD).write $(RUNTIME_JFLASH_HEX)
	cd $(BOOTLOADER_BUILD_DIR)/zephyr && $(JLINK_CMD).write $(BOOTLOADER_JFLASH_HEX)
	@echo "current time: $$(date +'%Y-%m-%d %H:%M:%S')"

.PHONY: jflash_write.application
jflash_write.application:
	$(JLINK_CMD).device $(JLINK_TARGET)
	@test -f "$(APPLICATION_FLASH_DIR)/$(APPLICATION_FLASH_HEX)" || { echo "Missing $(APPLICATION_FLASH_DIR)/$(APPLICATION_FLASH_HEX). Rebuild with application for BOARD_PROFILE=$(BOARD_PROFILE)." >&2; exit 1; }
	cd $(APPLICATION_FLASH_DIR) && $(JLINK_CMD).write $(APPLICATION_FLASH_HEX)
	@echo "current time: $$(date +'%Y-%m-%d %H:%M:%S')"

.PHONY: arduino.version
arduino.version: builder.image
	$(DOCKER_RUN) arduino-cli version

.PHONY: arduino.package.local
arduino.package.local:
	rm -rf $(ARDUINO_PACKAGE_DIR)
	mkdir -p $(ARDUINO_PACKAGE_DIR)
	cp -a cores variants libraries system boards.txt platform.txt programmers.txt $(ARDUINO_PACKAGE_DIR)/

.PHONY: package.rtbus.index
package.rtbus.index:
	$(if $(strip $(ARDUINO_PACKAGE_BASE_URL)),scripts/package_rtbus_index.sh $(ARDUINO_PACKAGE_BASE_URL),scripts/package_rtbus_index.sh)

.PHONY: arduino.package.index
arduino.package.index: package.rtbus.index

.PHONY: arduino.boards
arduino.boards: builder.image arduino.package.local
	$(DOCKER_RUN) arduino-cli --config-file $(ARDUINO_CONFIG) board listall rtduo

.PHONY: application
application: application.$(APPLICATION_BACKEND)

.PHONY: application.arduino
application.arduino: builder.image arduino.package.local
	@test -n "$(ARDUINO_BOARD_ID)" || { echo "Unsupported BOARD_PROFILE=$(BOARD_PROFILE)" >&2; exit 1; }
	$(DOCKER_RUN) arduino-cli --config-file $(ARDUINO_CONFIG) compile \
		--verbose \
		--fqbn $(ARDUINO_FQBN) \
		--build-path $(DOCKER_WORK)/$(ARDUINO_APPLICATION_BUILD_DIR) \
		$(ARDUINO_LOCAL_BUILD_PROPERTIES) \
		--build-property build.application.pack=true \
		$(DOCKER_WORK)/$(ARDUINO_SKETCH)

.PHONY: application.gcc
application.gcc: builder.image
	$(DOCKER_RUN) sh -ec 'rm -rf "$(APPLICATION_BUILD_TMP_DIR_DOCKER)"; $(MAKE) -f "$(DOCKER_WORK)/$(APPLICATION_GCC_MAKEFILE)" \
		BOARD_PROFILE=$(BOARD_PROFILE) \
		BUILD_DIR="$(APPLICATION_BUILD_TMP_DIR_DOCKER)" \
		SRC="$(DOCKER_WORK)/$(APPLICATION_SOURCE)" \
		APPLICATION_VERSION="$(APPLICATION_VERSION)" \
		APPLICATION_BUILD="$(APPLICATION_BUILD)" \
		GCC_PATH="$(APPLICATION_GCC_PATH)"; rm -rf "$(APPLICATION_BUILD_DIR_DOCKER)"; mkdir -p "$$(dirname "$(APPLICATION_BUILD_DIR_DOCKER)")"; cp -a "$(APPLICATION_BUILD_TMP_DIR_DOCKER)" "$(APPLICATION_BUILD_DIR_DOCKER)"'

.PHONY: application.clean
application.clean: application.$(APPLICATION_BACKEND).clean

.PHONY: application.arduino.clean
application.arduino.clean:
	rm -rf $(ARDUINO_APPLICATION_BUILD_DIR)

.PHONY: application.gcc.clean
application.gcc.clean:
	$(MAKE) -f $(APPLICATION_GCC_MAKEFILE) \
		BOARD_PROFILE=$(BOARD_PROFILE) \
		BUILD_DIR=$(abspath $(APPLICATION_BUILD_DIR)) \
		clean

application.%:
	@echo "Unsupported APPLICATION_BACKEND=$*. Use one of: arduino gcc" >&2
	@exit 1

.PHONY: arduino.clean
arduino.clean:
	rm -rf $(ARDUINO_BUILD_ROOT)
