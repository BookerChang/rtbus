# SPDX-License-Identifier: MPL-2.0

GITHUB_CI_DOCKER_RUN = $(VM) run --rm \
	-v $(CURDIR):$(DOCKER_WORK) \
	-v $(RTBUS_ZEPHYR_VOLUME):$(RTBUS_ZEPHYR_WORKSPACE) \
	--tmpfs $(DOCKER_WORK)/.west \
	-w $(DOCKER_WORK) \
	-e RTBUS_ZEPHYR_WORKSPACE=$(RTBUS_ZEPHYR_WORKSPACE) \
	-e ZEPHYR_BASE=$(RTBUS_ZEPHYR_WORKSPACE)/zephyr \
	-e ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-$(ZEPHYR_SDK_VERSION) \
	-e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	-e GH_TOKEN \
	$(DOCKER_IMAGE)

.PHONY: ci.github.gh
ci.github.gh: docker.image
	@test -n "$(GH_TOKEN)" || { echo "GH_TOKEN is required" >&2; exit 1; }
	$(GITHUB_CI_DOCKER_RUN) gh $(GH_ARGS)
