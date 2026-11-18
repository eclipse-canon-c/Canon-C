/*
 * cortexm.ld -- the memory of the tests under QEMU with newlib
 * (tools/cortexm/cortexm.py, VERIFY-089): 4 MB of code at 0x00000000 and
 * 4 MB of RAM at 0x20000000, which mps2-an386 has and the micro:bit is
 * given (its nRF51's sram-size and flash-size). QEMU loads every section
 * where it is linked, so .data is not copied at start.
 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}
ENTRY(canon_reset_handler)
SECTIONS
{
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text : {
        *(.text*) *(.rodata*)
        KEEP(*(.init)) KEEP(*(.fini))
        . = ALIGN(4);
        __preinit_array_start = .; KEEP(*(.preinit_array)) __preinit_array_end = .;
        __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .;
        __fini_array_start = .; KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) __fini_array_end = .;
    } > FLASH
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH
    .data : { *(.data*) } > RAM
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM
    . = ALIGN(8);
    end = .;
    PROVIDE(_end = .);
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
