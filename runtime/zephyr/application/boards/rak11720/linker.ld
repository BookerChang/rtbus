/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * Link layout for the RAK11720 upgradable application image.
 */

ENTRY(__rtduo_start)

MEMORY
{
    MODULE_FLASH (rx)  : ORIGIN = 0x000EC080, LENGTH = 40832
    MODULE_RAM   (rwx) : ORIGIN = 0x1005DF00, LENGTH = 8192
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

    __module_entry = __rtduo_start;
    __module_slot_start = ORIGIN(MODULE_FLASH) - 0x80;
    __module_flash_start = ORIGIN(MODULE_FLASH);
    __module_flash_end = ORIGIN(MODULE_FLASH) + LENGTH(MODULE_FLASH);
    __module_ram_start = ORIGIN(MODULE_RAM);
    __module_ram_end = ORIGIN(MODULE_RAM) + LENGTH(MODULE_RAM);
    __rtbus_native_api_table_base = 0x1005FF00;
}

ASSERT(__module_data_end <= __module_ram_end, "module RAM overflow")
ASSERT(__module_bss_end <= __module_ram_end, "module RAM overflow")
