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

ENTRY(_start)

SECTIONS
{
    /* ----------------------------------------------------------------
     * Bootloader text + data start at 0x80000
     * (where the GPU places kernel8.img)
     * ---------------------------------------------------------------- */
    . = 0x80000;

    /* ---- .text.boot must be first (contains _start) ---- */
    .text.boot : {
        KEEP(*(.text.boot))
    }

    /* ---- Exception vector table - must be 0x800-aligned ---- */
    . = ALIGN(0x800);
    .vectors : {
        KEEP(*(.vectors))
    }

    /* ---- Regular code ---- */
    . = ALIGN(8);
    .text : {
        *(.text)
        *(.text.*)
    }

    /* ---- Read-only data ---- */
    . = ALIGN(8);
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* ---- Initialised data ---- */
    . = ALIGN(8);
    .data : {
        *(.data)
        *(.data.*)
    }

    /* ---- BSS (zero-initialised) ---- */
    . = ALIGN(8);
    __bss_start = .;
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    . = ALIGN(8);
    __bss_end = .;

    /* ---- Discard unnecessary sections ---- */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.gnu.attributes)
        *(.ARM.attributes)
    }
}

/* ----------------------------------------------------------------
 * Exported symbols used in start.S
 * ---------------------------------------------------------------- */
PROVIDE(_stack_top = 0x80000);
