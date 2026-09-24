RUNTIME_APP_DIR := runtime/zephyr/runtime
RUNTIME_BOARD_DIR ?= cores/$(BOARD_PROFILE)/zephyr
BOARD_RUNTIME_CONF ?= $(RUNTIME_BOARD_DIR)/runtime.conf
BOARD_RUNTIME_OVERLAY ?= $(RUNTIME_BOARD_DIR)/runtime.overlay
RUNTIME_BUILD_DIR ?= build.zephyr/runtime/$(BOARD_PROFILE)
RUNTIME_SHARE_DIR ?= zephyr-share/runtime/$(BOARD_PROFILE)
RUNTIME_CONTAINER_BUILD_DIR ?= $(RTBUS_CONTAINER_BUILD_ROOT)/runtime/$(BOARD_PROFILE)

RUNTIME_ZEPHYR_MODULES := $(strip $(RTBUS_ZEPHYR_MODULES))

RUNTIME_EXTRA_CONF_FILE :=
ifneq ($(strip $(BOARD_RUNTIME_CONF)),)
RUNTIME_EXTRA_CONF_FILE := $(DOCKER_WORK)/$(BOARD_RUNTIME_CONF)
endif
ifneq ($(strip $(RTBUS_EXTRA_CONF)),)
RUNTIME_EXTRA_CONF_FILE := $(RUNTIME_EXTRA_CONF_FILE)$(if $(strip $(RUNTIME_EXTRA_CONF_FILE)),;,)$(RTBUS_EXTRA_CONF)
endif

RUNTIME_DTC_OVERLAY_FILE :=
ifneq ($(strip $(BOARD_RUNTIME_OVERLAY)),)
RUNTIME_DTC_OVERLAY_FILE := $(DOCKER_WORK)/$(BOARD_RUNTIME_OVERLAY)
endif
ifneq ($(strip $(RTBUS_EXTRA_OVERLAY)),)
RUNTIME_DTC_OVERLAY_FILE := $(RUNTIME_DTC_OVERLAY_FILE)$(if $(strip $(RUNTIME_DTC_OVERLAY_FILE)),;,)$(RTBUS_EXTRA_OVERLAY)
endif

RUNTIME_BOARD_ARGS := -DEXTRA_CONF_FILE="$(RUNTIME_EXTRA_CONF_FILE)"
ifneq ($(strip $(RUNTIME_DTC_OVERLAY_FILE)),)
RUNTIME_BOARD_ARGS += -DDTC_OVERLAY_FILE="$(RUNTIME_DTC_OVERLAY_FILE)"
endif
ifneq ($(strip $(BOARD_ROOT_ARG)),)
RUNTIME_BOARD_ARGS += $(BOARD_ROOT_ARG)
endif
ifneq ($(strip $(RTBUS_BOARD_ROOTS)),)
RUNTIME_BOARD_ARGS += -DBOARD_ROOT="$(RTBUS_BOARD_ROOTS)"
endif
ifneq ($(strip $(RUNTIME_ZEPHYR_MODULES)),)
RUNTIME_BOARD_ARGS += -DZEPHYR_EXTRA_MODULES="$(RUNTIME_ZEPHYR_MODULES)"
endif

.PHONY: runtime
runtime: builder.image zephyr.workspace
	@test -n "$(ZEPHYR_BOARD)" || { echo "Unsupported BOARD_PROFILE=$(BOARD_PROFILE)" >&2; exit 1; }
	$(DOCKER_RUN) sh -ec 'rm -rf "$(RUNTIME_CONTAINER_BUILD_DIR)" "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)"; cd "$(RTBUS_ZEPHYR_WORKSPACE)" && west build -b "$(ZEPHYR_BOARD)" -d "$(RUNTIME_CONTAINER_BUILD_DIR)" -s "$(DOCKER_WORK)/$(RUNTIME_APP_DIR)" -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(RUNTIME_BOARD_ARGS); mkdir -p "$$(dirname "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)")"; cp -a "$(RUNTIME_CONTAINER_BUILD_DIR)" "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)"; rm -rf "$(DOCKER_WORK)/$(RUNTIME_SHARE_DIR)"; mkdir -p "$(DOCKER_WORK)/$(RUNTIME_SHARE_DIR)"; for f in zephyr.hex zephyr.bin zephyr.signed.hex zephyr.signed.bin zephyr.elf zephyr.map zephyr.dts .config; do if [ -f "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)/zephyr/$$f" ]; then cp "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)/zephyr/$$f" "$(DOCKER_WORK)/$(RUNTIME_SHARE_DIR)/$$f"; fi; done; if [ -f "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)/compile_commands.json" ]; then cp "$(DOCKER_WORK)/$(RUNTIME_BUILD_DIR)/compile_commands.json" "$(DOCKER_WORK)/$(RUNTIME_SHARE_DIR)/compile_commands.json"; fi; echo "Runtime artifacts exported: $(DOCKER_WORK)/$(RUNTIME_SHARE_DIR)"'
	$(MAKE) package.rtbus.index

.PHONY: runtime.clean
runtime.clean:
	rm -rf build.zephyr/runtime zephyr-share/runtime
