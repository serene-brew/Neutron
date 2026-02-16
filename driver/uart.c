/* =============================================================================
 * Neutron Bootloader - Project Atom
 * drivers/uart.c  —  PL011 UART driver (ARMv7 bare-metal)
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 * =============================================================================
 */

#include "uart.h"
#include <stdarg.h>
#include <stdint.h>

/* ---------- PL011 register offsets (all 32-bit wide) -----------------------
 */
#define UART_DR 0x000   /* Data Register                               */
#define UART_RSR 0x004  /* Receive Status / Error Clear Register       */
#define UART_FR 0x018   /* Flag Register                               */
#define UART_IBRD 0x024 /* Integer   Baud Rate Divisor                 */
#define UART_FBRD 0x028 /* Fractional Baud Rate Divisor                */
#define UART_LCRH 0x02C /* Line Control Register H                     */
#define UART_CR 0x030   /* Control Register                            */
#define UART_IMSC 0x038 /* Interrupt Mask Set/Clear                    */
#define UART_ICR 0x044  /* Interrupt Clear Register                    */

/* Flag Register bits */
#define FR_TXFF (1 << 5) /* TX FIFO full                             */
#define FR_RXFE (1 << 4) /* RX FIFO empty                            */
#define FR_BUSY (1 << 3) /* UART busy transmitting                   */

/* Line Control Register H bits */
#define LCRH_WLEN_8 (0x3 << 5) /* 8-bit word length                       */
#define LCRH_FEN (1 << 4)      /* FIFO enable                              */

/* Control Register bits */
#define CR_UARTEN (1 << 0) /* UART enable                              */
#define CR_TXE (1 << 8)    /* TX enable                                */
#define CR_RXE (1 << 9)    /* RX enable                                */

/* ---------- Module-level state ---------------------------------------------
 */
static volatile uint32_t *uart_base = (volatile uint32_t *)0;

static inline void reg_write(uint32_t offset, uint32_t val) {
  *((volatile uint32_t *)((uint8_t *)uart_base + offset)) = val;
}

static inline uint32_t reg_read(uint32_t offset) {
  return *((volatile uint32_t *)((uint8_t *)uart_base + offset));
}

/* ---------- Division helper (avoids libgcc __aeabi_uidiv/__aeabi_uidivmod) --
 * Restoring-subtraction unsigned 32-bit divide.
 * Returns quotient; stores remainder in *rem (if rem != NULL).
 * Only called a small number of times so the loop cost doesn't matter.     */
static uint32_t udivmod(uint32_t dividend, uint32_t divisor, uint32_t *rem) {
  uint32_t quotient = 0;
  uint32_t d = divisor;
  int shift = 0;

  /* Shift divisor left until it exceeds dividend or hits MSB */
  while ((d & 0x80000000UL) == 0 && d <= (dividend >> 1)) {
    d <<= 1;
    shift++;
  }
  /* Subtract from the highest aligned bit down */
  for (int i = shift; i >= 0; i--) {
    uint32_t trial = divisor << i;
    if (trial <= dividend) {
      dividend -= trial;
      quotient |= (1UL << i);
    }
  }
  if (rem)
    *rem = dividend;
  return quotient;
}

/* ---------- Public API -----------------------------------------------------
 */

void uart_init(uint32_t base_addr, uint32_t uart_clk, uint32_t baud) {
  uart_base = (volatile uint32_t *)(uintptr_t)base_addr;

  /* 1. Disable the UART while we reconfigure it                             */
  reg_write(UART_CR, 0);

  /* 2. Wait for any ongoing transmission to finish                          */
  while (reg_read(UART_FR) & FR_BUSY)
    ;

  /* 3. Flush and disable the FIFOs                                          */
  reg_write(UART_LCRH, reg_read(UART_LCRH) & ~LCRH_FEN);

  /* 4. Calculate baud rate divisors
   *    BRD      = uart_clk / (16 * baud)
   *    brd×64   = (uart_clk × 4) / baud        (avoid __aeabi_uidiv)
   *    IBRD     = brd×64 >> 6
   *    FBRD     = brd×64 & 0x3F                                              */
  uint32_t brd_times_64 = udivmod(uart_clk * 4, baud, (void *)0);
  uint32_t ibrd = brd_times_64 >> 6;
  uint32_t fbrd = brd_times_64 & 0x3F;

  reg_write(UART_IBRD, ibrd);
  reg_write(UART_FBRD, fbrd);

  /* 5. 8N1, FIFO enabled                                                    */
  reg_write(UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);

  /* 6. Mask all interrupts (polled driver for now)                          */
  reg_write(UART_IMSC, 0);
  reg_write(UART_ICR, 0x7FF); /* clear any pending interrupts            */

  /* 7. Enable UART, TX and RX                                               */
  reg_write(UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

void uart_putc(char c) {
  /* Translate \n to \r\n for terminal emulators                             */
  if (c == '\n')
    uart_putc('\r');

  /* Spin while TX FIFO is full                                              */
  while (reg_read(UART_FR) & FR_TXFF)
    ;

  reg_write(UART_DR, (uint32_t)(uint8_t)c);
}

char uart_getc(void) {
  /* Spin while RX FIFO is empty                                             */
  while (reg_read(UART_FR) & FR_RXFE)
    ;

  return (char)(reg_read(UART_DR) & 0xFF);
}

void uart_puts(const char *str) {
  while (*str)
    uart_putc(*str++);
}

/* ---------- Minimal uart_printf --------------------------------------------
 */

static void print_uint(uint32_t n, uint32_t base, int pad, char pad_char) {
  static const char digits[] = "0123456789abcdef";
  char buf[32];
  int i = 0;

  if (n == 0) {
    buf[i++] = '0';
  } else if (base == 16) {
    /* Hex: use shifts and masks — zero division risk, no libgcc needed  */
    while (n) {
      buf[i++] = digits[n & 0xF];
      n >>= 4;
    }
  } else {
    /* Decimal (base 10): use our own udivmod, not / or %                */
    while (n) {
      uint32_t rem;
      n = udivmod(n, 10, &rem);
      buf[i++] = digits[rem];
    }
  }

  /* Padding */
  while (i < pad)
    buf[i++] = pad_char;

  /* Print reversed (we built the string least-significant digit first)   */
  while (i--)
    uart_putc(buf[i]);
}

static void print_int(int32_t n) {
  if (n < 0) {
    uart_putc('-');
    n = -n;
  }
  print_uint((uint32_t)n, 10, 0, ' ');
}

void uart_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      uart_putc(*fmt++);
      continue;
    }

    fmt++; /* skip '%' */

    /* Optional zero-padding: e.g. %08x */
    char pad_char = ' ';
    int pad_width = 0;
    if (*fmt == '0') {
      pad_char = '0';
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9') {
      pad_width = pad_width * 10 + (*fmt - '0');
      fmt++;
    }

    switch (*fmt) {
    case 'c':
      uart_putc((char)va_arg(args, int));
      break;
    case 's': {
      const char *s = va_arg(args, const char *);
      uart_puts(s ? s : "(null)");
      break;
    }
    case 'd':
    case 'i':
      print_int(va_arg(args, int32_t));
      break;
    case 'u':
      print_uint(va_arg(args, uint32_t), 10, pad_width, pad_char);
      break;
    case 'x':
    case 'X':
      print_uint(va_arg(args, uint32_t), 16, pad_width, pad_char);
      break;
    case 'p':
      uart_puts("0x");
      print_uint((uint32_t)(uintptr_t)va_arg(args, void *), 16, 8, '0');
      break;
    case '%':
      uart_putc('%');
      break;
    default:
      uart_putc('%');
      uart_putc(*fmt);
      break;
    }
    fmt++;
  }

  va_end(args);
}
