/* =============================================================================
 * Neutron Bootloader - Project Atom
 * test_kernel/boot/kernel_start.S  -  Minimal Test Kernel Entry Point
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 *
 * The Neutron bootloader jumps here with:
 *   x0 = physical address of boot_info_t  (at 0x1000)
 *
 * This runs at EL1, MMU off, caches off.
 * Stack is set up just below our load address (0x200000).
 * ================================================================ */

.section ".text.kernel_entry"
.global kernel_start

kernel_start:
    /* Save boot_info pointer (x0) before we clobber registers */
    mov     x19, x0

    /* Set up a small stack just below our load address */
    ldr     x0, =0x1F0000          /* 128 KiB below 0x200000 */
    mov     sp, x0

    /* Pass boot_info pointer as first argument to kernel_main() */
    mov     x0, x19
    bl      kernel_main

    /* kernel_main() should never return - park here if it does */
_kernel_park:
    wfe
    b       _kernel_park
