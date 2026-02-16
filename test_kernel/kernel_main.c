/* =============================================================================
 * Neutron Bootloader - Project Atom
 * test_kernel/kernel_main.c  —  Minimal test kernel for bootloader testing
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * This is a very simple kernel that:
 * 1. Initializes UART
 * 2. Prints a greeting message
 * 3. Loops forever
 *
 * Used for testing the bootloader's kernel loading and jumping mechanism
 * ============================================================================= */

#include "uart.h"
#include <stdint.h>

#define UART0_BASE 0x09000000UL
#define UART_CLK_HZ 24000000UL
#define UART_BAUD 115200UL

/* Simple putchar for kernel */
static void kputc(char c) {
    uart_putc(c);
}

/* Simple puts for kernel */
static void kputs(const char *s) {
    while (*s)
        kputc(*s++);
}

/* ============================================================================
 * Print 64-bit hex value
 * ============================================================================ */
static void print_hex64(uint64_t value) {
    for (int i = 60; i >= 0; i -= 4) {
        unsigned int nibble = (value >> i) & 0xF;
        kputc("0123456789abcdef"[nibble]);
    }
}

/* ============================================================================
 * kernel_main — Main kernel entry point
 *
 * Called from _start assembly after CPU is initialized.
 * x0 contains the DTB address passed from bootloader.
 * ============================================================================ */
void kernel_main(uint64_t dtb_addr) {
    /* Initialize UART immediately */
    uart_init(UART0_BASE, UART_CLK_HZ, UART_BAUD);

    /* Print first message */
    kputs("\r\n--------------------------------------------\r\n");
    kputs("\r\n[KERNEL] Started!\r\n");
    kputs("[KERNEL] Hello from the kernel!\r\n");
    kputs("[KERNEL] Kernel entry point: 0x40200000\r\n");
    kputs("[KERNEL] DTB address (device tree): 0x");
    print_hex64(dtb_addr);
    kputs("\r\n\r\n");

    kputs("[KERNEL] Bootloader successfully transferred control!\r\n");
    kputs("[KERNEL] Kernel is running in EL1 mode.\r\n");
    kputs("[KERNEL] Looping forever...\r\n\r\n");

    /* Loop forever */
    while (1) {
        /* Kernel would normally:
         * - Handle interrupts
         * - Schedule processes
         * - Manage memory
         * - etc.
         */
        asm volatile("wfi");  /* Wait for interrupt */
    }
}
