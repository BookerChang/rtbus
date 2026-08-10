PROJECT := rtbus

VM ?= podman
DOCKER_IMAGE ?= localhost/rtbus-zephyr:arm-1.0.0
DOCKER_WORK ?= /workdir
ARDUINO_CONFIG ?= arduino-cli.yaml
ARDUINO_PACKAGE_ROOT ?= build.arduino/package
ARDUINO_PACKAGE_DIR ?= $(ARDUINO_PACKAGE_ROOT)/hardware/rtbus/rtduo
ARDUINO_SKETCH ?= libraries/RTDuo/examples/HelloWorld
ARDUINO_BUILD_ROOT ?= build.arduino

BOARD_PROFILE ?= rak4631
ARDUINO_BOARD_ID_rak4631 := RAK4631
ARDUINO_BOARD_ID_rak3172 := RAK3172
ARDUINO_BOARD_ID_rak3172f := RAK3172F
ARDUINO_BOARD_ID_rak3172p := RAK3172P
ARDUINO_BOARD_ID_rak3172t := RAK3172T
ARDUINO_BOARD_ID_rak11720 := RAK11720
ARDUINO_BOARD_ID_rak4200 := RAK4200
ARDUINO_BOARD_ID ?= $(ARDUINO_BOARD_ID_$(BOARD_PROFILE))
ARDUINO_FQBN ?= rtbus:rtduo:$(ARDUINO_BOARD_ID)
ARDUINO_APPLICATION_BUILD_DIR ?= $(ARDUINO_BUILD_ROOT)/$(BOARD_PROFILE)/application

ZEPHYR_SDK_VERSION ?= 1.0.0
ARDUINO_LOCAL_COMPILER_PATH ?= /opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION)/gnu/arm-zephyr-eabi/bin/
ARDUINO_LOCAL_HOST_COMPILER_PATH ?= /usr/bin/
ARDUINO_LOCAL_HOST_COMPILER_CMD ?= gcc

include docker/docker.mk

DOCKER_RUN = $(VM) run --rm -v $(CURDIR):$(DOCKER_WORK) -w $(DOCKER_WORK) $(DOCKER_IMAGE)
ARDUINO_LOCAL_BUILD_PROPERTIES = \
	--build-property compiler.path=$(ARDUINO_LOCAL_COMPILER_PATH) \
	--build-property compiler.host.path=$(ARDUINO_LOCAL_HOST_COMPILER_PATH) \
	--build-property compiler.host.cmd=$(ARDUINO_LOCAL_HOST_COMPILER_CMD) \
	--build-property compiler.host.flags=

.DEFAULT_GOAL := arduino.boards

.PHONY: builder.image
builder.image: docker.image

.PHONY: arduino.version
arduino.version: builder.image
	$(DOCKER_RUN) arduino-cli version

.PHONY: arduino.package.local
arduino.package.local:
	rm -rf $(ARDUINO_PACKAGE_DIR)
	mkdir -p $(ARDUINO_PACKAGE_DIR)
	cp -a cores variants libraries system boards.txt platform.txt programmers.txt $(ARDUINO_PACKAGE_DIR)/

.PHONY: arduino.boards
arduino.boards: builder.image arduino.package.local
	$(DOCKER_RUN) arduino-cli --config-file $(ARDUINO_CONFIG) board listall rtduo

.PHONY: arduino.compile
arduino.compile: builder.image arduino.package.local
	@test -n "$(ARDUINO_BOARD_ID)" || { echo "Unsupported BOARD_PROFILE=$(BOARD_PROFILE)" >&2; exit 1; }
	$(DOCKER_RUN) arduino-cli --config-file $(ARDUINO_CONFIG) compile \
		--verbose \
		--fqbn $(ARDUINO_FQBN) \
		--build-path $(DOCKER_WORK)/$(ARDUINO_APPLICATION_BUILD_DIR) \
		$(ARDUINO_LOCAL_BUILD_PROPERTIES) \
		--build-property build.application.pack=true \
		$(DOCKER_WORK)/$(ARDUINO_SKETCH)

.PHONY: arduino.clean
arduino.clean:
	rm -rf $(ARDUINO_BUILD_ROOT)
