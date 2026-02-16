/* =============================================================================
 * Neutron Bootloader - Project Atom
 * kernel/load.c  —  Kernel loading and jumping logic for bootloader
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * The bootloader is responsible for:
 * - Locating the kernel image in storage
 * - Loading it to KERNEL_BASE (0x40200000)
 * - Jumping to it with DTB address in x0
 * ============================================================================= */

#include "bootloader.h"
#include "uart.h"
#include <stdint.h>
#include <stddef.h>

/* Bootloader Information Global ============================================ */
bootloader_info_t bootloader_info = {0};

/* ============================================================================
 * bootloader_info_init — Initialize bootloader info with DTB address
 * ============================================================================ */
void bootloader_info_init(uint64_t dtb_addr) {
    bootloader_info.dtb_phys_addr     = dtb_addr;
    bootloader_info.bootloader_version = 0x00010000;  /* v1.0 */
    bootloader_info.bootloader_flags   = 0;
}

/* ============================================================================
 * bootloader_load_kernel — Load kernel from staging area to KERNEL_BASE
 *
 * For QEMU -machine virt:
 * - Use QEMU's "loader" device to place kernel at KERNEL_IMAGE_LOAD_ADDR (0x40400000)
 * - Copy from there to KERNEL_BASE (0x40200000)
 *
 * For real hardware:
 * - Load from SD card, eMMC, USB, etc. to KERNEL_BASE
 * - Validate kernel magic/checksum
 *
 * Returns:
 *   0 on success
 *   -1 on failure (kernel not found, invalid, etc.)
 * ============================================================================ */
int bootloader_load_kernel(void) {
    /* For QEMU: User provides kernel via:
     *   qemu-system-aarch64 -kernel bootloader.elf \
     *                       -device loader,file=kernel.elf,addr=0x40400000
     *
     * The kernel ELF is loaded at 0x40400000 by the loader device
     * We copy it to KERNEL_BASE (0x40200000) and then jump to it
     */

    bootloader_printf("[BOOT] Loading kernel from staging area 0x%016x...\r\n", KERNEL_IMAGE_LOAD_ADDR);

    /* DEBUG: Check what's at the staging area */
    volatile uint32_t *src_check = (volatile uint32_t *)KERNEL_IMAGE_LOAD_ADDR;
    uint32_t first_word = src_check[0];
    bootloader_printf("[BOOT] First word at staging: 0x%08x\r\n", first_word);

    /* Copy kernel from KERNEL_IMAGE_LOAD_ADDR to KERNEL_BASE
     *
     * In production, you would:
     * 1. Validate ELF header at KERNEL_IMAGE_LOAD_ADDR
     * 2. Use ELF program headers to determine what to copy where
     * 3. Check kernel magic/checksum
     * 4. Handle errors gracefully
     */

    uint8_t *src = (uint8_t *)KERNEL_IMAGE_LOAD_ADDR;
    uint8_t *dst = (uint8_t *)KERNEL_BASE;

    bootloader_printf("[BOOT] Copying kernel to final address 0x%016x\r\n", KERNEL_BASE);

    /* Simple byte-by-byte copy (slow but works for bootloader)
     * Production code would use memcpy or DMA
     *
     * TODO: Implement proper ELF loading instead of flat binary copy
     */
    volatile uint32_t *src32 = (volatile uint32_t *)src;
    volatile uint32_t *dst32 = (volatile uint32_t *)dst;

    /* Copy a reasonable amount - at least 64 KB to cover typical small kernels
     * A production bootloader would parse the ELF header to get the actual size
     */
    size_t copy_size = 0x10000;  /* 64 KB - adjust as needed for larger kernels */

    for (size_t i = 0; i < copy_size / 4; i++) {
        dst32[i] = src32[i];
    }

    /* DEBUG: Verify the copy worked */
    volatile uint32_t *dst_check = (volatile uint32_t *)KERNEL_BASE;
    uint32_t copied_word = dst_check[0];
    bootloader_printf("[BOOT] First word at destination: 0x%08x\r\n", copied_word);

    bootloader_printf("[BOOT] Kernel loaded: %u bytes copied\r\n", copy_size);

    return 0;  /* Success */
}

/* ============================================================================
 * bootloader_jump_to_kernel — Jump to kernel with DTB address in x0
 *
 * This function never returns. It sets up the execution environment and
 * transfers control to the kernel at KERNEL_BASE.
 * ============================================================================ */
void bootloader_jump_to_kernel(uint64_t dtb_addr) {
    bootloader_printf("[BOOT] Jumping to kernel at 0x%016x\r\n", KERNEL_BASE);
    bootloader_printf("[BOOT] DTB address in x0: 0x%016x\r\n", dtb_addr);
    bootloader_printf("[BOOT] Entering kernel...\r\n\r\n");

    /* Ensure all UART output is flushed before jumping */
    volatile int delay = 1000000;
    while (--delay)
        ;

    /* Jump to kernel entry point with DTB in x0
     *
     * ARM AArch64 ABI: x0 is the first argument
     * x0 = DTB address for kernel to use for device tree
     */
    asm volatile(
        "mov x0, %0\n"      /* x0 = DTB address                    */
        "br %1\n"           /* Branch to kernel (no return)        */
        : : "r"(dtb_addr), "r"(KERNEL_BASE)
        : "x0"
    );

    /* Never reached */
    while (1)
        ;
}
