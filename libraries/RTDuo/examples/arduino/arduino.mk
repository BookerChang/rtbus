THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
THIS_DIR := $(dir $(abspath $(THIS_MAKEFILE)))
REPO_ROOT ?= $(abspath $(THIS_DIR)/../../../..)
BOARD_PROFILE ?= rak4631
ARDUINO_SKETCH := libraries/RTDuo/examples/arduino

.PHONY: compile
compile:
	$(MAKE) -C $(REPO_ROOT) application \
		BOARD_PROFILE=$(BOARD_PROFILE) \
		ARDUINO_SKETCH=$(ARDUINO_SKETCH)

.PHONY: boards
boards:
	$(MAKE) -C $(REPO_ROOT) arduino.boards

.PHONY: version
version:
	$(MAKE) -C $(REPO_ROOT) arduino.version

.PHONY: clean
clean:
	$(MAKE) -C $(REPO_ROOT) arduino.clean
