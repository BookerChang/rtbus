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

The goal is to form RTBus as a Zephyr subsystem. The Zephyr runtime application
should stay thin, while RTBus-owned runtime behavior lives under
`runtime/zephyr/modules/rtbus/`. Do not move RTBus runtime loop or native
service bootstrap logic back into `runtime/zephyr/runtime/src/main.c`.

Current RTBus subsystem split:

- `services/rtbus_runtime.c`: owns the static Zephyr thread that calls
  `rtbus_init()` and runs the `rtbus_process()` loop.
- `services/rtbus_bootstrap.c`: handles RTBus system bootstrap events, including
  starting the native service from the `RTBUS_SYS_INIT` event.
- `services/native_service/`: owns native application loading, ABI table setup,
  and the native application thread.

Expected boot flow:

1. Zephyr starts the static RTBus runtime thread from `rtbus_runtime.c`.
2. That thread calls `rtbus_init()`.
3. EMOS/RTBus posts `RTBUS_SYS_INIT` to registered init tasks.
4. `rtbus_bootstrap.c` receives that event and calls `native_service_start()`.
5. The native service creates/runs the native application thread.

The runtime app `main.c` is intentionally minimal and may only return `0`.

`dummy_service.c` has been removed. Its former role is now represented by
`rtbus_bootstrap.c`, and the task id name is `APPLICATION_TASK_NATIVE_BOOTSTRAP`.

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
- If a name is ABI-facing or shared with native applications, verify the ABI
  impact before renaming it. Prefer internal RTBus/native naming only behind
  compatibility layers.
- `native_service` naming is intentional for the runtime-side service that
  loads and runs the native application.
- `WISNODEZ_API_SLOT_*`, `WZ_API_*`, and other ABI-facing names may still exist
  for compatibility. Do not convert them to RTBus names unless the ABI migration
  is explicitly part of the task.
- The native application owns its own RAM region. Avoid adding Zephyr heap usage
  unless there is a concrete need.

## Verification

Use the existing board profile build targets. The usual smoke test for runtime
changes is:

```sh
make runtime BOARD_PROFILE=rak4631
```

If build output is needed, artifacts are exported under:

```text
zephyr-share/runtime/<board>/
```
