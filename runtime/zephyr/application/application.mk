APPLICATION_BUILD_DIR ?= $(ZEPHYR_BUILD_ROOT)/application/$(BOARD_PROFILE)
APPLICATION_BUILD_TMP_DIR ?= $(ZEPHYR_BUILD_TMP_ROOT)/application/$(BOARD_PROFILE)
APPLICATION_BUILD_DIR_DOCKER := $(call docker_path,$(APPLICATION_BUILD_DIR))
APPLICATION_BUILD_TMP_DIR_DOCKER := $(call docker_path,$(APPLICATION_BUILD_TMP_DIR))
APPLICATION_SOURCE ?= $(APPLICATION_APP_DIR)/main.c
APPLICATION_GCC_PATH ?= /opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION)/gnu/arm-zephyr-eabi/bin
APPLICATION_VERSION_FILE ?= system/IMGTOOL_VERSION
APPLICATION_BUILD_FILE ?= system/IMGTOOL_VERSION.build

define ensure_application_version
	@mkdir -p $(dir $(APPLICATION_BUILD_FILE)); \
	if [ ! -f "$(APPLICATION_VERSION_FILE)" ]; then \
		echo "Missing $(APPLICATION_VERSION_FILE)" >&2; exit 1; \
	fi; \
	if [ ! -f "$(APPLICATION_BUILD_FILE)" ]; then \
		printf '0\n' > "$(APPLICATION_BUILD_FILE)"; \
		printf 'Initialized %s with 0\n' "$(APPLICATION_BUILD_FILE)"; \
	fi
endef

.PHONY: application
application: builder.image
	$(ensure_application_version)
	$(DOCKER_RUN) sh -ec 'rm -rf "$(APPLICATION_BUILD_TMP_DIR_DOCKER)"; $(MAKE) -C "$(DOCKER_WORK)/$(APPLICATION_APP_DIR)" \
		BOARD_PROFILE=$(BOARD_PROFILE) \
		BUILD_DIR="$(APPLICATION_BUILD_TMP_DIR_DOCKER)" \
		SRC="$(DOCKER_WORK)/$(APPLICATION_SOURCE)" \
		GCC_PATH="$(APPLICATION_GCC_PATH)"; rm -rf "$(APPLICATION_BUILD_DIR_DOCKER)"; mkdir -p "$$(dirname "$(APPLICATION_BUILD_DIR_DOCKER)")"; cp -a "$(APPLICATION_BUILD_TMP_DIR_DOCKER)" "$(APPLICATION_BUILD_DIR_DOCKER)"'

.PHONY: application.clean
application.clean:
	$(MAKE) -C $(APPLICATION_APP_DIR) \
		BOARD_PROFILE=$(BOARD_PROFILE) \
		BUILD_DIR=$(abspath $(APPLICATION_BUILD_DIR)) \
		clean
