/* =============================================================================
 * Neutron Bootloader - Project Atom
 * linker/bootloader.ld  —  ARMv8 AArch64 Bootloader  /  QEMU -machine virt
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * RAM base      : 0x40000000
 * Bootloader    : 0x40000000  (loaded by firmware/QEMU)
 * Kernel        : 0x40200000  (loaded by bootloader)
 * ============================================================================= */

BOOTLOADER_LOAD_ADDR = 0x40000000;
STACK_SIZE       = 0x10000;      /* 64 KiB */

ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = BOOTLOADER_LOAD_ADDR, LENGTH = 2M
}

PHDRS {
    text PT_LOAD FLAGS(5);   /* R + X (read + execute) */
    data PT_LOAD FLAGS(6);   /* R + W (read + write) */
    bss  PT_LOAD FLAGS(6);   /* R + W (read + write) */
}

SECTIONS {
    . = BOOTLOADER_LOAD_ADDR;
    _bootloader_start = .;

    .text : {
        *(.text.boot)       /* _start must be first */
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

    _bootloader_end = .;

    /DISCARD/ : {
        *(.note.*) *(.comment) *(.gnu.*) *(.ARM.exidx*) *(.ARM.extab*)
    }
}
