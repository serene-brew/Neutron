/* =============================================================================
 * Neutron Bootloader - Project Atom
 * include/uart.h  —  PL011 UART driver interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * The ARM PrimeCell PL011 UART is present on:
 *   - QEMU vexpress-a9  at 0x10009000 (UART0)
 *   - QEMU virt         at 0x09000000 (UART0)
 * =============================================================================
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Call once to configure baud rate and enable the UART.
 * base_addr : MMIO base address of the PL011 controller
 * uart_clk  : input clock frequency in Hz (typically 24 MHz on vexpress)
 * baud      : desired baud rate (e.g. 115200)                              */
void uart_init(uint32_t base_addr, uint32_t uart_clk, uint32_t baud);

/* Transmit a single byte (blocking).                                        */
void uart_putc(char c);

/* Receive a single byte (blocking).                                         */
char uart_getc(void);

/* Transmit a NUL-terminated string.                                         */
void uart_puts(const char *str);

/* Minimal printf-like helper (supports %c %s %d %u %x %p).                */
void uart_printf(const char *fmt, ...);

#endif /* UART_H */
