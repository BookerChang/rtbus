/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Link layout for the RAK3172F upgradable application image.
 */

ENTRY(main)

MEMORY
{
    MODULE_FLASH (rx)  : ORIGIN = 0x08031080, LENGTH = 40832
    MODULE_RAM   (rwx) : ORIGIN = 0x2000EF00, LENGTH = 4096
}

SECTIONS
{
    .text :
    {
        __module_text_start = .;
        *(.text*)
        *(.rodata*)
        __module_text_end = .;
    } > MODULE_FLASH

    .data :
    {
        __module_data_start = .;
        *(.sdata*)
        *(.data*)
        __module_data_end = .;
    } > MODULE_RAM AT> MODULE_FLASH
    __module_data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        __module_bss_start = .;
        *(.sbss*)
        *(.bss*)
        *(COMMON)
        __module_bss_end = .;
    } > MODULE_RAM

    __module_entry = main;
    __module_slot_start = ORIGIN(MODULE_FLASH) - 0x80;
    __module_flash_start = ORIGIN(MODULE_FLASH);
    __module_flash_end = ORIGIN(MODULE_FLASH) + LENGTH(MODULE_FLASH);
    __module_ram_start = ORIGIN(MODULE_RAM);
    __module_ram_end = ORIGIN(MODULE_RAM) + LENGTH(MODULE_RAM);
    __wisnodez_api_table_base = 0x2000FF00;
}

ASSERT(__module_data_end <= __module_ram_end, "module RAM overflow")
ASSERT(__module_bss_end <= __module_ram_end, "module RAM overflow")
