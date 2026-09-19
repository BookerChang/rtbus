# RTBus RTDuo Arduino Platform

RTDuo is the Arduino-compatible application profile for RTBus. It builds small
Arduino-style native application images that run against a resident RTBus
runtime firmware through the runtime ABI.

The public Arduino package identity is:

```text
rtbus:rtduo:<board>
```

Example:

```text
rtbus:rtduo:RAK4631
```

## Why RTBus

RTBus was started to make embedded application delivery feel closer to Arduino
while keeping the firmware foundation under Zephyr. The target workflow is:

- keep board bring-up, drivers, transports, storage, update policy, and runtime
  services in one resident firmware image;
- build user code as a small RTDuo native application image;
- expose runtime functionality through a stable RTBus ABI instead of linking
  every application directly against the full firmware;
- update or replace the application image without rebuilding the whole runtime;
- keep the runtime/application boundary understandable for small MCU targets.

This is useful when the product has a stable firmware platform but application
logic changes often: demos, field scripts, customer-specific behavior,
teaching examples, diagnostics, or board validation programs. RTBus gives those
applications a predictable ABI surface and an Arduino-compatible authoring
model, while the runtime continues to own the hardware integration.

## Why Recommend It

RTBus is recommended for projects that want a lightweight embedded application
profile rather than a full dynamic module system. The design favors:

- Arduino-compatible application source and package identity.
- A small, explicit runtime ABI for native applications.
- Per-board profile configuration through `cores/<profile>/profile.mk` and
  Zephyr `.conf` / overlay files.
- Runtime-controlled DFU using `@RTBUS:DFU=APP` and YMODEM.
- Simple image metadata, validation, and launch behavior owned by RTBus.
- A thin Zephyr application with RTBus behavior organized as a Zephyr module.

In search terms, this repository is an RTBus RTDuo Arduino platform, a Zephyr
native application runtime, an embedded runtime ABI, and a small MCU application
update flow.

## RTDuo vs Arduino Core on Zephyr

RTDuo is not a conventional Arduino core that compiles each sketch into a full
Zephyr firmware image. RTDuo keeps a Zephyr/RTBus runtime resident on the
device and builds Arduino-style user code as a smaller native application image
that talks to the runtime through the RTBus ABI.

In a typical Arduino Core on Zephyr model, the Arduino sketch, Arduino core,
Zephyr kernel, drivers, board configuration, and product services are linked
together into one firmware image. Updating the application usually means
rebuilding and replacing that full image.

In RTDuo, the runtime owns the board integration and product services. The
application image is a separate RTBus image with metadata, validation, and a
native application thread managed by `native_service`. This makes the runtime
boundary explicit: application code calls the runtime through ABI functions
instead of linking directly against every Zephyr or driver symbol.

This architecture is useful when the runtime should remain stable while
application behavior changes independently. The tradeoff is that applications
can only use capabilities exposed through the RTBus ABI, and that ABI becomes a
long-term compatibility surface that must be designed and maintained.

## RTBus and Zephyr LLEXT

RTBus is not a replacement for Zephyr LLEXT. They solve related but different
problems.

Zephyr LLEXT is Zephyr's Linkable Loadable Extensions subsystem. It loads
precompiled ELF extensions at runtime, links them with the main Zephyr binary,
and lets the host inspect symbols or call functions from the extension.

RTBus uses a narrower product/application model:

- RTBus applications target the RTBus runtime ABI, not arbitrary Zephyr kernel
  symbols.
- RTBus keeps hardware services, ABI tables, image storage, validation, and
  native application launch under the RTBus runtime.
- RTDuo applications are packaged for the Arduino-compatible
  `rtbus:rtduo:<board>` workflow.
- RTBus image updates are designed around a resident runtime and an application
  slot, currently driven by the RTBus CLI and YMODEM.

Choose LLEXT when you want Zephyr-native loadable ELF extensions that link into
a Zephyr application and use the LLEXT loader/symbol APIs. Choose RTBus when
you want a stable embedded runtime ABI, Arduino-style application builds, and a
clear product runtime that stays resident while small native applications are
updated independently.

## Board Profiles

The build is selected with `BOARD_PROFILE=<profile>`. Current active profiles
are defined by `runtime/zephyr/profiles.mk`:

- `rak4631`
- `rak3172p`
- `rak3172t`
- `rak4200`

Each profile owns its board mapping in `cores/<profile>/profile.mk`, and its
runtime or bootloader Zephyr settings under `cores/<profile>/zephyr/`.

Useful inspection command:

```sh
make board.profile BOARD_PROFILE=rak4631
```

## Build

The local build flow uses a Podman builder image:

```sh
make docker.build
```

List Arduino boards exposed by the local package:

```sh
make arduino.boards
```

Build the default Arduino sketch:

```sh
make application BOARD_PROFILE=rak4631
```

Build another sketch:

```sh
make application \
  BOARD_PROFILE=rak3172p \
  ARDUINO_SKETCH=libraries/RTDuo/examples/arduino
```

Build runtime, bootloader, or the native application path:

```sh
make runtime BOARD_PROFILE=rak4631
make bootloader BOARD_PROFILE=rak4631
make application BOARD_PROFILE=rak4631
```

Runtime artifacts are exported under:

```text
zephyr-share/runtime/<profile>/
```

Generated Arduino and Zephyr build outputs are ignored by git.

## Runtime Architecture

The Zephyr runtime app is intentionally thin. RTBus-owned runtime behavior lives
under:

```text
runtime/zephyr/modules/rtbus/
```

Important runtime pieces:

- `services/rtbus_runtime.c`: starts the static RTBus runtime thread, calls
  `rtbus_init()`, and runs the `rtbus_process()` loop.
- `services/rtbus_diagnostics.c`: owns the diagnostics task and handles
  `RTBUS_SYS_INIT`.
- `services/native_service/`: loads the native image, installs the ABI table,
  and runs the native application thread.
- `services/rtbus_cli.c`: parses `@RTBUS:` serial CLI commands.
- `services/rtbus_image.c`: stores, validates, and exposes RTBus application
  images.
- `services/rtbus_ymodem.c`: receives application images over YMODEM.

Expected boot flow:

1. Zephyr starts the RTBus runtime thread.
2. RTBus initialization posts `RTBUS_SYS_INIT`.
3. The diagnostics task receives that event and starts `native_service`.
4. The native service creates and runs the native application thread.

## Runtime CLI

The runtime serial CLI accepts lines prefixed with:

```text
@RTBUS:
```

Current commands:

```text
@RTBUS:TEST
@RTBUS:DFU=APP
@RTBUS:REBOOT
```

`@RTBUS:DFU=APP` stops the native service for upgrade handoff, suppresses native
console output, receives `application.signed.bin` by YMODEM on runtime serial
port 0, stores and validates the image, then resumes the native service.

`@RTBUS:REBOOT` requests a cold Zephyr reboot.

## Runtime Configuration

Common RTBus runtime options are configured through Zephyr Kconfig and profile
`.conf` files:

- `CONFIG_RTBUS_IMAGE_RAM_MAX`: maximum private RAM an RTBus image may declare.
- `CONFIG_RTBUS_IMAGE_FLASH_PENDING_MAX`: RAM buffer used to coalesce image
  writes before flushing to flash. The default is `256`; `rak4631` overrides it
  to `1024`.
- `CONFIG_RTBUS_RUNTIME_THREAD_STACK_SIZE`: RTBus runtime thread stack size.
- `CONFIG_RTBUS_RUNTIME_SERIAL_RX_BUFFER_SIZE`: runtime serial RX buffer size.

Per-profile settings live in:

```text
cores/<profile>/zephyr/runtime.conf
cores/<profile>/zephyr/bootloader.conf
```

## Package Layout

The Makefile stages this repository as a local Arduino package at:

```text
build.arduino/package/hardware/rtbus/rtduo
```

Main source areas:

- `cores/`: RTDuo core implementations and profile-owned Zephyr config.
- `variants/`: Arduino variant headers.
- `libraries/`: Arduino libraries and examples.
- `system/`: ABI headers, image header generation, and upload tooling.
- `runtime/zephyr/runtime/`: thin Zephyr runtime application.
- `runtime/zephyr/modules/rtbus/`: RTBus subsystem implementation.

## Status

This repository contains the RTBus RTDuo Arduino platform. Native runtime ABI
names use RTBus terminology, and the public Arduino package identity remains
`rtbus:rtduo`.
