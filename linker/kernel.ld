/* =============================================================================
 * Neutron Bootloader - Project Atom
 * linker/kernel.ld  —  Kernel linker script for ARMv8 AArch64
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Kernel loads at: 0x40200000
 * ============================================================================= */

KERNEL_LOAD_ADDR = 0x40200000;
STACK_SIZE = 0x10000;  /* 64 KiB */

ENTRY(kernel_entry)

MEMORY {
    RAM (rwx) : ORIGIN = KERNEL_LOAD_ADDR, LENGTH = 16M
}

PHDRS {
    text PT_LOAD FLAGS(5);   /* R + X (read + execute) */
    data PT_LOAD FLAGS(6);   /* R + W (read + write) */
    bss  PT_LOAD FLAGS(6);   /* R + W (read + write) */
}

SECTIONS {
    . = KERNEL_LOAD_ADDR;
    _kernel_start = .;

    .text : {
        *(.text.kernel_boot)  /* kernel_entry must be first */
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
    } > RAM : text

    . = ALIGN(8);
    _data_load_start = LOADADDR(.data);

    .data : {
        _data_start = .;
        *(.data .data.*)
        . = ALIGN(8);
        _data_end = .;
    } > RAM : data

    . = ALIGN(8);
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM : bss

    . = ALIGN(16);
    .stack (NOLOAD) : {
        _stack_bottom = .;
        . += STACK_SIZE;
        _stack_top = .;
    } > RAM : bss

    _kernel_end = .;

    /DISCARD/ : {
        *(.note.*) *(.comment) *(.gnu.*) *(.ARM.exidx*) *(.ARM.extab*)
    }
}
