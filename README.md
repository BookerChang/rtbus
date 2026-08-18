# RTBus RTDuo Arduino Platform

RTDuo is the Arduino-compatible application profile for RTBus. It builds a
small Arduino-style application image that talks to a resident runtime firmware
through the RTBus runtime ABI.

The Arduino package identity is:

```text
rtbus:rtduo:<board>
```

For example:

```text
rtbus:rtduo:RAK4631
```

## Supported Boards

- RAK4631
- RAK3172
- RAK3172F
- RAK3172P
- RAK3172T
- RAK11720
- RAK4200

## Build

The local build flow uses `arduino-cli` from the Podman builder image
`localhost/rtbus-zephyr:arm-1.0.0`.

Build the local builder image first:

```bash
make docker.build
```

Inspect or enter the builder image:

```bash
make docker.images
make docker.shell
```

```bash
make arduino.version
make arduino.boards
make arduino.compile BOARD_PROFILE=rak4631
```

To compile another sketch:

```bash
make arduino.compile \
  BOARD_PROFILE=rak3172f \
  ARDUINO_SKETCH=libraries/RTDuo/examples/arduino
```

Build the baseline Zephyr runtime:

```bash
make zephyr.workspace
make runtime BOARD_PROFILE=rak4631
```

Generated Arduino cache and build outputs are ignored by git.

## Runtime

The open-source runtime baseline lives under:

```text
runtime/zephyr/
```

It currently includes:

- `runtime`: the RTBus Zephyr runtime firmware
- `bootloader`: the RTBus MCUboot integration and board bootloader profiles
- `application`: the standalone native C application build path
- `modules/mod_schedule`: EMOS scheduler support
- `modules/mod_fw_component`: the application image descriptor/loader primitive
- `boards`: board profiles used to validate the baseline runtime

Downstream transports and product-specific update logic, such as LoRaWAN,
FUOTA, patch handling, credentials, and region policy, are intentionally not
part of the upstream baseline. Those should be added by downstream projects as
Zephyr modules/config overlays.

Build the runtime or bootloader with an explicit board profile:

```sh
make runtime BOARD_PROFILE=rak4631
make bootloader BOARD_PROFILE=rak4631
make application BOARD_PROFILE=rak4631
```

## Package Layout

The Makefile stages this repository as a local Arduino package at:

```text
build.arduino/package/hardware/rtbus/rtduo
```

## Status

This repository is an early extraction of the Arduino base from the WisNodeZ
prototype work. Some low-level ABI symbols still keep their original WisNodeZ
names for runtime compatibility while the public Arduino package identity has
been moved to `rtbus:rtduo`.
