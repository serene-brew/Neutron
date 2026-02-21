/* =============================================================================
 * Neutron Bootloader - Project Atom
 * driver/uart.c  -  PL011 UART0 driver
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * RPi Zero 2W physical pins:
 *   GPIO14  TXD0  (ALT0)
 *   GPIO15  RXD0  (ALT0)
 *
 * Baud rate calculation for 48 MHz UART clock, 115200 baud:
 *   Divisor = 48000000 / (16 * 115200) = 26.042
 *   IBRD    = 26
 *   FBRD    = round(0.042 * 64) = 3
 * ================================================================ */

#include "uart.h"
#include "gpio.h"
#include "platform.h"
#include <stdarg.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * MMIO helpers
 * ---------------------------------------------------------------- */
static inline void mmio_write(uintptr_t addr, uint32_t val) {
  *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read(uintptr_t addr) {
  return *(volatile uint32_t *)addr;
}

/* ----------------------------------------------------------------
 * uart_init()
 *   1. Disable UART
 *   2. Configure GPIO 14/15 as ALT0 (TXD0/RXD0), no pull
 *   3. Set baud rate divisors
 *   4. Enable UART: 8N1, FIFO, TX+RX
 * ---------------------------------------------------------------- */
void uart_init(void) {
  /* Step 1: Disable UART */
  mmio_write(UART0_BASE + UART_CR, 0);

  /* Step 2: Configure GPIO pins */
  gpio_set_func(14, GPIO_FUNC_ALT0); /* TXD0 */
  gpio_set_func(15, GPIO_FUNC_ALT0); /* RXD0 */
  gpio_set_pull(14, GPIO_PULL_NONE);
  gpio_set_pull(15, GPIO_PULL_NONE);

  /* Step 3: Clear all pending interrupts */
  mmio_write(UART0_BASE + UART_ICR, 0x7FF);

  /* Step 4: Baud rate  115200  @  48 MHz UART clock
   *   IBRD = 26, FBRD = 3                              */
  mmio_write(UART0_BASE + UART_IBRD, 26);
  mmio_write(UART0_BASE + UART_FBRD, 3);

  /* Step 5: 8 data bits, 1 stop, no parity, enable FIFO */
  mmio_write(UART0_BASE + UART_LCRH, UART_LCRH_WLEN_8 | UART_LCRH_FEN);

  /* Step 6: Mask all interrupts */
  mmio_write(UART0_BASE + UART_IMSC, 0);

  /* Step 7: Enable UART - TX + RX */
  mmio_write(UART0_BASE + UART_CR, UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE);
}

/* ----------------------------------------------------------------
 * uart_putc()  -  blocking transmit one character
 * ---------------------------------------------------------------- */
void uart_putc(char c) {
  /* CR to CRLF translation for terminals */
  if (c == '\n')
    uart_putc('\r');

  /* Wait until TX FIFO has space */
  while (mmio_read(UART0_BASE + UART_FR) & UART_FR_TXFF)
    ;

  mmio_write(UART0_BASE + UART_DR, (uint32_t)(unsigned char)c);
}

/* ----------------------------------------------------------------
 * uart_puts()
 * ---------------------------------------------------------------- */
void uart_puts(const char *s) {
  while (*s)
    uart_putc(*s++);
}

/* ----------------------------------------------------------------
 * uart_getc()  -  blocking receive
 * ---------------------------------------------------------------- */
char uart_getc(void) {
  while (mmio_read(UART0_BASE + UART_FR) & UART_FR_RXFE)
    ;
  return (char)(mmio_read(UART0_BASE + UART_DR) & 0xFF);
}

/* ----------------------------------------------------------------
 * uart_puthex64 / uart_puthex32 / uart_putdec
 * ---------------------------------------------------------------- */
static const char hex_chars[] = "0123456789ABCDEF";

/* Internal: digits only, no "0x" prefix - used by uart_printf */
static void _puthex64_digits(uint64_t val) {
  for (int i = 60; i >= 0; i -= 4)
    uart_putc(hex_chars[(val >> i) & 0xF]);
}
static void _puthex32_digits(uint32_t val) {
  for (int i = 28; i >= 0; i -= 4)
    uart_putc(hex_chars[(val >> i) & 0xF]);
}

/* Public: includes "0x" prefix */
void uart_puthex64(uint64_t val) {
  uart_puts("0x");
  _puthex64_digits(val);
}

void uart_puthex32(uint32_t val) {
  uart_puts("0x");
  _puthex32_digits(val);
}

void uart_putdec(uint64_t val) {
  if (val == 0) {
    uart_putc('0');
    return;
  }
  char buf[20];
  int idx = 0;
  while (val) {
    buf[idx++] = '0' + (val % 10);
    val /= 10;
  }
  for (int i = idx - 1; i >= 0; i--)
    uart_putc(buf[i]);
}

/* ----------------------------------------------------------------
 * uart_printf()  -  lightweight printf subset
 *   Supports: %s %c %d %u %x %X %p  (%% for literal %)
 *   No width/precision specifiers (keep it small).
 * ---------------------------------------------------------------- */
void uart_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      uart_putc(*fmt++);
      continue;
    }
    fmt++; /* skip '%' */
    /* Skip optional width/flag characters: 0-9, '-', '+', ' ', '0' */
    while (*fmt == '-' || *fmt == '+' || *fmt == ' ' ||
           (*fmt >= '0' && *fmt <= '9'))
      fmt++;
    switch (*fmt++) {
    case 's': {
      const char *s = va_arg(args, const char *);
      if (!s)
        s = "(null)";
      uart_puts(s);
      break;
    }
    case 'c': {
      char c = (char)va_arg(args, int);
      uart_putc(c);
      break;
    }
    case 'd': {
      int64_t v = va_arg(args, int64_t);
      if (v < 0) {
        uart_putc('-');
        v = -v;
      }
      uart_putdec((uint64_t)v);
      break;
    }
    case 'u': {
      uint64_t v = va_arg(args, uint64_t);
      uart_putdec(v);
      break;
    }
    case 'x': {
      uint64_t v = va_arg(args, uint64_t);
      _puthex64_digits(v);
      break;
    }
    case 'X': {
      uint32_t v = va_arg(args, uint32_t);
      _puthex32_digits(v);
      break;
    }
    case 'p': {
      uintptr_t v = va_arg(args, uintptr_t);
      uart_puts("0x");
      _puthex64_digits((uint64_t)v);
      break;
    }
    case '%':
      uart_putc('%');
      break;
    default:
      uart_putc('?');
      break;
    }
  }

  va_end(args);
}
