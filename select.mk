BOARD_PROFILE_PROMPT_GOALS := \
	board.profile \
	runtime \
	bootloader \
	application \
	application.clean \
	jflash_erase \
	jflash_write.runtime \
	jflash_write.application \
	arduino.compile

BOARD_PROFILE_REQUESTED := $(filter $(BOARD_PROFILE_PROMPT_GOALS),$(MAKECMDGOALS))

define select_board_profile
$(strip $(shell \
	tty_path=`tty 2>/dev/null`; \
	if [ -z "$$tty_path" ] || [ "$$tty_path" = "not a tty" ]; then \
		exit 0; \
	fi; \
	printf 'Select BOARD_PROFILE:\n' > "$$tty_path"; \
	index=1; \
	for board in $(BOARD_PROFILE_CHOICES); do \
		printf '  %s. %s\n' "$$index" "$$board" > "$$tty_path"; \
		index=$$(($$index + 1)); \
	done; \
	printf 'BOARD_PROFILE [1]: ' > "$$tty_path"; \
	read answer < "$$tty_path"; \
	index=1; \
	for board in $(BOARD_PROFILE_CHOICES); do \
		if [ -z "$$answer" ] && [ "$$index" = "1" ]; then \
			printf '%s\n' "$$board"; \
			exit 0; \
		fi; \
		if [ "$$answer" = "$$index" ] || [ "$$answer" = "$$board" ]; then \
			printf '%s\n' "$$board"; \
			exit 0; \
		fi; \
		index=$$(($$index + 1)); \
	done; \
	printf '__invalid__%s\n' "$$answer"))
endef

ifeq ($(strip $(BOARD_PROFILE)),)
ifneq ($(BOARD_PROFILE_REQUESTED),)
BOARD_PROFILE := $(call select_board_profile)
ifeq ($(strip $(BOARD_PROFILE)),)
$(error BOARD_PROFILE is required for $(BOARD_PROFILE_REQUESTED). Use one of: $(BOARD_PROFILE_CHOICES))
endif
ifneq ($(filter __invalid__%,$(BOARD_PROFILE)),)
$(error Invalid BOARD_PROFILE selection: $(patsubst __invalid__%,%,$(BOARD_PROFILE)))
endif
endif
endif
