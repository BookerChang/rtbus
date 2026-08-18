# Agent Instructions

## Project

This repository is the RTBus RTDuo Arduino platform.

RTDuo is the Arduino-compatible application profile for RTBus. It builds small
Arduino-style application images that communicate with a resident RTBus runtime
firmware through the runtime ABI.

The Arduino package identity is:

```text
rtbus:rtduo:<board>
```

Example:

```text
rtbus:rtduo:RAK4631
```

## Current Focus

The main ongoing work is integrating the application service into the RTBus
module layer. Keep this direction in mind when changing runtime/application
boundaries, build rules, ABI-facing code, or module structure.

## Main Areas

- `cores/`, `variants/`, `libraries/`, `system/`: Arduino platform sources.
- `runtime/zephyr/`: open-source Zephyr runtime baseline.
- `runtime/zephyr/runtime/`: RTBus runtime firmware.
- `runtime/zephyr/bootloader/`: MCUboot integration and bootloader profiles.
- `runtime/zephyr/application/`: standalone native C application build path.
- `bootloader/`, `zephyr/`, `modules/`: upstream or vendored platform pieces.
- `build.arduino/`, `build.zephyr/`, `build.zephyr.tmp/`: generated build output.

## Supported Board Profiles

- `rak4631`
- `rak3172`
- `rak3172f`
- `rak3172p`
- `rak3172t`
- `rak11720`
- `rak4200`

Use `BOARD_PROFILE=<profile>` for board-specific build targets.

## Common Commands

Build the Podman builder image:

```sh
make docker.build
```

List Arduino boards:

```sh
make arduino.boards
```

Compile the default Arduino sketch:

```sh
make arduino.compile BOARD_PROFILE=rak4631
```

Build Zephyr runtime, bootloader, or native application:

```sh
make runtime BOARD_PROFILE=rak4631
make bootloader BOARD_PROFILE=rak4631
make application BOARD_PROFILE=rak4631
```

Inspect selected board profile variables:

```sh
make board.profile BOARD_PROFILE=rak4631
```

## Editing Guidance

- Keep changes focused on the requested RTBus/RTDuo behavior.
- Prefer existing Makefile targets and board profile patterns over adding new
  build flows.
- Do not edit generated build output unless explicitly requested.
- Treat upstream or vendored trees such as `zephyr/`, `modules/`, and MCUboot
  code as higher risk; keep changes there minimal and justified.
- Preserve the public package identity `rtbus:rtduo`.
- Some low-level ABI names may still contain old WisNodeZ naming for runtime
  compatibility; do not rename ABI symbols casually.
