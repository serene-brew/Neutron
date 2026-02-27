/* =============================================================================
 * Neutron Bootloader - Project Atom
 * include/uart.h  —  PL011 UART driver interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * The ARM PrimeCell PL011 UART is present on:
 *   - QEMU raspi3b at 0x3F201000 (MMIO)
 * =============================================================================
 */

#ifndef UART_H
#define UART_H

/* ================================================================
 * include/uart.h  -  PL011 UART driver interface
 * ================================================================ */

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex64(uint64_t val);
void uart_puthex32(uint32_t val);
void uart_putdec(uint64_t val);
char uart_getc(void);

/* Printf-lite: supports %s %c %x %X %d %u %p  (no floats) */
void uart_printf(const char *fmt, ...);

#endif /* UART_H */
