# RTBus Zephyr Profiles

`BOARD_PROFILE` is the RTBus product-level board selection. It is not always
the same as the Zephyr board name.

Each `cores/<profile>/profile.mk` file owns the metadata that explains where
the Zephyr board support comes from:

- `PROFILE_NAME`: RTBus profile name.
- `ZEPHYR_BOARD`: board name passed to `west build -b`.
- `ZEPHYR_BOARD_SOURCE`: `upstream` for Zephyr-provided boards, `project` for
  boards provided by this repository.
- `ZEPHYR_BOARD_VARIANT_OF`: optional base profile or board when this profile is
  a hardware variant.
- `ZEPHYR_SOC`: SoC used by the Zephyr board.
- `JLINK_TARGET`: SEGGER J-Link target name.
- `BOARD_ROOTS`: optional Zephyr board root list. Project-owned boards should
  keep their board tree under the owning core, for example
  `cores/rak4200/zephyr/boards/rakwireless/rak4200`.

Runtime, bootloader, profile, and project-owned Zephyr board files live under
the owning `cores/<profile>` directory. The profile layer is the single place
for board identity and source mapping.
