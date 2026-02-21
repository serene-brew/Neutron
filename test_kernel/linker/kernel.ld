/* =============================================================================
 * Neutron Bootloader - Project Atom
 * test_kernel/linker/kernel.ld  -  Minimal Test Kernel Linker Script
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 *
 * The Neutron bootloader copies the packed kernel payload to
 * 0x200000 and jumps there.  So our VMA starts at 0x200000.
 * ================================================================ */

ENTRY(kernel_start)

SECTIONS
{
    . = 0x200000;

    /* Entry must be first */
    .text.kernel_entry : {
        KEEP(*(.text.kernel_entry))
    }

    .text : {
        *(.text)
        *(.text.*)
    }

    . = ALIGN(8);
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    . = ALIGN(8);
    .data : {
        *(.data)
        *(.data.*)
    }

    . = ALIGN(8);
    __kernel_bss_start = .;
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    . = ALIGN(8);
    __kernel_bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.attributes)
    }
}
