# RTBus Zephyr Boards

`BOARD_PROFILE` is the RTBus product-level board selection. It is not always
the same as the Zephyr board name.

Each board/profile directory owns the metadata that explains where the Zephyr
board support comes from:

- `PROFILE_NAME`: RTBus profile name.
- `ZEPHYR_BOARD`: board name passed to `west build -b`.
- `ZEPHYR_BOARD_SOURCE`: `upstream` for Zephyr-provided boards, `project` for
  boards provided by this repository.
- `ZEPHYR_BOARD_VARIANT_OF`: optional base profile or board when this profile is
  a hardware variant.
- `ZEPHYR_SOC`: SoC used by the Zephyr board.
- `JLINK_TARGET`: SEGGER J-Link target name.
- `BOARD_ROOTS`: optional Zephyr board root list. Project-owned boards should
  keep their board tree in the project-level `zephyr-boards` root, for example
  `zephyr-boards/boards/rakwireless/rak4200`.

Runtime, bootloader, and native application files still live in their existing
per-layer `boards/<profile>` directories. The profile layer is the single place
for board identity and source mapping; per-layer directories describe how each
firmware layer uses that profile.
