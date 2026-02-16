/* =============================================================================
 * Neutron Bootloader - Project Atom
 * test_kernel/kernel_start.S  —  Kernel entry point for bare-metal ARM AArch64
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * This is the kernel's bootstrap code, called by the bootloader.
 * The bootloader jumps here with:
 * - x0 = DTB address
 * - CPU in EL1 mode
 * - MMU disabled
 * - Stack already set up by bootloader (if needed)
 * ============================================================================= */

.section ".text.kernel_boot"
.global kernel_entry
.global _kernel_start

.align 3
_kernel_start:
kernel_entry:
    /* x0 contains DTB address from bootloader - SAVE IT FIRST! */
    mov x27, x0           /* Save DTB address in x27 (callee-saved) */

    /* Set up stack at a fixed location (use top of kernel RAM) */
    ldr x0, =0x40300000  /* Load stack address into x0 */
    mov sp, x0           /* Move to stack pointer */

    /* Restore DTB address to x0 for kernel_main */
    mov x0, x27           /* x0 = DTB address (restore from x27) */

    /* Jump to C code - x0 is first argument (DTB address) */
    bl kernel_main
kernel_hang:
    wfi
    b kernel_hang
