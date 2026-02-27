/* =============================================================================
 * Neutron Bootloader - Project Atom
 * neutron.h  -  Neutron Bootloader Shared ABI Header (ARMv8-A)
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause

 * This header defines the shared ABI between bootloader and kernel.
 * =============================================================================
*/

#ifndef NEUTRON_H
#define NEUTRON_H

#include <stdint.h>

/* =========================
 * Bootloader <-> Kernel ABI
 * ========================= */

/* Well-known boot info location (optional; kernel actually receives x0=boot_info*) */
#define BOOT_INFO_ADDR 0x1000UL

#define BOOT_INFO_MAGIC 0xB007B007U

typedef struct {
  uint32_t magic;          /* BOOT_INFO_MAGIC */
  uint32_t board_revision; /* from mailbox */
  uint32_t arm_mem_size;   /* ARM-accessible RAM bytes */
  uint32_t kernel_load_addr;
  uint32_t kernel_entry_addr;
  uint32_t kernel_size;
  char bootloader_version[16]; /* 15 chars + NUL */
} __attribute__((packed)) boot_info_t;

_Static_assert(sizeof(boot_info_t) == 40, "boot_info_t ABI size mismatch");

/* =========================
 * Common MMIO/platform defs
 * (used by both bootloader + test_kernel UART/GPIO code)
 * ========================= */

#define MMIO_BASE 0x3F000000UL

/* GPIO */
#define GPIO_BASE (MMIO_BASE + 0x00200000UL)
#define GPFSEL1 0x04
#define GPPUD 0x94
#define GPPUDCLK0 0x98

/* PL011 UART0 */
#define UART0_BASE (MMIO_BASE + 0x00201000UL)
#define UART_DR 0x00
#define UART_FR 0x18
#define UART_IBRD 0x24
#define UART_FBRD 0x28
#define UART_LCRH 0x2C
#define UART_CR 0x30
#define UART_IMSC 0x38
#define UART_ICR 0x44

/* UART FR bits */
#define UART_FR_BUSY (1 << 3)
#define UART_FR_RXFE (1 << 4)
#define UART_FR_TXFF (1 << 5)

/* UART LCRH bits */
#define UART_LCRH_FEN (1 << 4)
#define UART_LCRH_WLEN_8 (0x3 << 5)
/* Compatibility name used by test_kernel today */
#define UART_LCRH_8BIT UART_LCRH_WLEN_8

/* UART CR bits */
#define UART_CR_UARTEN (1 << 0)
#define UART_CR_TXE (1 << 8)
#define UART_CR_RXE (1 << 9)
/* Compatibility name used by test_kernel today */
#define UART_CR_EN UART_CR_UARTEN

#endif /* NEUTRON_H */