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
- `modules/rtbus`: RTBus runtime subsystem sources
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

## Application DFU

The runtime CLI starts native application upgrades with:

```text
@RTBUS:DFU=APP
```

Use `application.signed.bin` as the payload for this flow. A typical sequence is:

1. Build or select a signed native application image.
2. Open the runtime serial port at the board upload baud rate.
3. Send the CLI line `@RTBUS:DFU=APP` followed by CR/LF.
4. Wait for the runtime to enter YMODEM receive mode. The receiver sends `C`
   while waiting for the first YMODEM packet.
5. Send `application.signed.bin` with YMODEM.
6. Wait for the final runtime status before closing the serial port.

When the CLI command is accepted, the runtime suppresses native application
console output, stops the native service for the upgrade handoff, receives the
image through YMODEM on runtime serial port 0, then resumes the native service
after the image store finishes.

## Package Layout

The Makefile stages this repository as a local Arduino package at:

```text
build.arduino/package/hardware/rtbus/rtduo
```

## Status

This repository contains the RTBus RTDuo Arduino platform. The public Arduino
package identity is `rtbus:rtduo`, and native runtime ABI names use RTBus
terminology.
