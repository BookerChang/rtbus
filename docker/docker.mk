ifeq ($(OS),Windows_NT)
HOST_OS_WINDOWS := 1
DOCKER_EXE ?=
DEFAULT_DOCKER_EXE := C:/Program Files/Docker/Docker/resources/bin/docker.exe
ifeq ($(strip $(DOCKER_EXE)),)
DETECTED_DOCKER_EXE := $(strip $(shell where docker.exe 2>NUL))
ifeq ($(strip $(DETECTED_DOCKER_EXE)),)
DETECTED_DOCKER_EXE := $(strip $(shell if exist "$(DEFAULT_DOCKER_EXE)" echo $(DEFAULT_DOCKER_EXE)))
endif
else
DETECTED_DOCKER_EXE := $(DOCKER_EXE)
endif
VM = "$(DETECTED_DOCKER_EXE)"
define resolve_container_cli
	@if "$(DETECTED_DOCKER_EXE)"=="" (echo Docker CLI not found. Install Docker Desktop, add docker.exe to PATH, or pass DOCKER_EXE="C:/path/to/docker.exe". & exit /b 1)
	@echo Container CLI: $(VM)
endef
else
HOST_OS_WINDOWS := 0
VM ?= $(shell command -v podman >/dev/null 2>&1 && echo podman || echo docker)
define resolve_container_cli
	@command -v $(VM) >/dev/null 2>&1 || { echo "Container CLI not found: $(VM)" >&2; exit 1; }
	@echo Container CLI: $(VM)
endef
endif

ZEPHYR_SDK_VERSION ?= 1.0.0
ARDUINO_CLI_VERSION ?= 1.5.1
ZEPHYR_BASE_IMAGE ?= ghcr.io/embeddedcontainers/zephyr:arm-$(ZEPHYR_SDK_VERSION)SDK
RTBUS_BUILDER_DOCKERFILE ?= docker/Dockerfile.embedded-arm
RTBUS_BUILDER_IMAGE ?= localhost/rtbus-zephyr:arm-$(ZEPHYR_SDK_VERSION)
DOCKER_IMAGE ?= $(RTBUS_BUILDER_IMAGE)
DOCKER_WORK ?= /workdir

.PHONY: docker.build
docker.build:
	$(call resolve_container_cli)
	$(VM) build \
		--build-arg EMBEDDED_ZEPHYR_IMAGE=$(ZEPHYR_BASE_IMAGE) \
		--build-arg ZEPHYR_SDK_VERSION=$(ZEPHYR_SDK_VERSION) \
		--build-arg ARDUINO_CLI_VERSION=$(ARDUINO_CLI_VERSION) \
		-t $(RTBUS_BUILDER_IMAGE) \
		-f $(RTBUS_BUILDER_DOCKERFILE) .

.PHONY: docker.image
docker.image:
	$(call resolve_container_cli)
	@$(VM) image inspect $(DOCKER_IMAGE) >/dev/null 2>&1 || { \
		echo "Missing builder image: $(DOCKER_IMAGE)" >&2; \
		echo "Build it with: make docker.build" >&2; \
		exit 1; \
	}

.PHONY: docker.images
docker.images:
	$(call resolve_container_cli)
	$(VM) images | grep -E 'rtbus-zephyr|embeddedcontainers/zephyr' || true

.PHONY: docker.shell
docker.shell: docker.image
	$(VM) run -it --user root --rm \
		-v $(CURDIR):$(DOCKER_WORK) \
		-w $(DOCKER_WORK) \
		-e ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION) \
		-e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
		$(DOCKER_IMAGE)
