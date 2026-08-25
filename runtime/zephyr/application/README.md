# RTBus Application Makefile

This directory keeps the standalone native application build path for the
RTBus Zephyr runtime tree.

Use this path when you want to build a C application directly with the
Zephyr SDK toolchain, without Arduino CLI sketch preprocessing.

## Build From Repository Root

```bash
make application BOARD_PROFILE=rak4631
make application BOARD_PROFILE=rak3172
make application BOARD_PROFILE=rak3172f
```

The root target is defined by `application.mk` and delegates to this
directory's standalone `Makefile`.

## Build From This Directory

Run commands from this directory.

RAK4631:

```bash
make BOARD_PROFILE=rak4631
```

RAK3172:

```bash
make BOARD_PROFILE=rak3172
```

## Build Another Source

The default source is:

```text
./main.c
```

Override `SRC` to build another native C application:

```bash
make BOARD_PROFILE=rak4631 SRC=/path/to/main.c
```

## Linker Layout

Standalone application builds use board-local linker entry files:

```text
boards/rak4631/linker.ld
boards/rak3172/linker.ld
boards/rak3172f/linker.ld
```

These files include the canonical Arduino core linker scripts through paths
relative to the `runtime/zephyr/application` working directory. Keep flash/RAM
layout changes in the core linker script unless the standalone application path
needs a board-specific override.

The root `make application` target uses `APPLICATION_VERSION=0.1.0` and
`APPLICATION_BUILD=0` by default. Override those variables when release metadata
needs to be embedded in the standalone native application image.

## Outputs

Build output is written to:

```text
../../build.rtbus.application.<board>/
```

Important files:

```text
application.elf
application.bin
application.payload.bin
application.signed.bin
application.signed.hex
slot_address.txt
```

Use `application.signed.bin` for the application DFU flow. Use
`application.signed.hex` when directly programming the application slot.

## Clean

```bash
make application.clean BOARD_PROFILE=rak4631
```

Or from this directory:

```bash
make BOARD_PROFILE=rak4631 clean
```
