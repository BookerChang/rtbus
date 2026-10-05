# SPDX-License-Identifier: MPL-2.0

BOARD_PROFILE_ROOT := cores
BOARD_PROFILE_ORDER := rak4631 rak3172p rak3172t rak4200
BOARD_PROFILE_CHOICES := $(strip $(foreach profile,$(BOARD_PROFILE_ORDER),$(if $(wildcard $(BOARD_PROFILE_ROOT)/$(profile)/profile.mk),$(profile))))
