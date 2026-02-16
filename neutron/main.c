/* =============================================================================
 * Neutron Bootloader - Project Atom
 * kernel/main.c  —  ARMv8 AArch64 bare-metal bootloader
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * This is the main bootloader code. It:
 * 1. Initializes UART for debug output
 * 2. Prints bootloader information
 * 3. Loads kernel from storage/memory
 * 4. Jumps to kernel at 0x40200000
 *
 * QEMU -machine virt:
 *   UART0 (PL011) : 0x09000000
 * =============================================================================
 */
#include "bootloader.h"
#include "uart.h"
#include <stdarg.h>
#include <stdint.h>

#define UART0_BASE 0x09000000UL
#define UART_CLK_HZ 24000000UL
#define UART_BAUD 115200UL

extern uint64_t _bootloader_start, _bootloader_end;
extern uint64_t _bss_start, _bss_end;
extern uint64_t _stack_top;
/* ---- Dual output ---------------------------------------------------------- */
static void bputc(char c) {
    uart_putc(c);
}

static void bputs(const char *s) {
    while (*s)
        bputc(*s++);
}

/* ---- Minimal udiv64 -------------------------------------------------------
 */
static uint64_t udiv64(uint64_t n, uint64_t d, uint64_t *r) {
  uint64_t q = 0, dd = d;
  int s = 0;
  while (!(dd & (1ULL << 63)) && dd <= (n >> 1)) {
    dd <<= 1;
    s++;
  }
  for (int i = s; i >= 0; i--)
    if ((d << i) <= n) {
      n -= d << i;
      q |= 1ULL << i;
    }
  if (r)
    *r = n;
  return q;
}
/* ============================================================================
 * bootloader_sleep — Sleep for specified milliseconds
 *
 * Simple busy-wait sleep function for timing delays in bootloader
 * ============================================================================ */
void bootloader_sleep(uint32_t milliseconds) {
    /* Approximate sleep using a loop
     * At ~50 million loops per second, ~50,000 loops per ms on cortex-a53
     */
    volatile uint32_t loops_per_ms = 50000;
    for (uint32_t ms = 0; ms < milliseconds; ms++) {
        for (volatile uint32_t i = 0; i < loops_per_ms; i++) {
            asm volatile("nop");
        }
    }
}

/* ---- kprintf --------------------------------------------------------------
 */
static void print_u64(uint64_t n, int base, int pad, char pc) {
    char buf[20];
    int i = 0;
    if (!n) {
        buf[i++] = '0';
    } else if (base == 16) {
        while (n) {
            buf[i++] = "0123456789abcdef"[n & 0xf];
            n >>= 4;
        }
    } else {
        while (n) {
            uint64_t r;
            n = udiv64(n, 10, &r);
            buf[i++] = '0' + r;
        }
    }
    while (i < pad)
        buf[i++] = pc;
    while (i--)
        bputc(buf[i]);
}

/* Bootloader printf with minimal format support ============================= */
void bootloader_printf(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            bputc(*fmt++);
            continue;
        }
        fmt++;
        char pc = ' ';
        int pw = 0;
        if (*fmt == '0') {
            pc = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            pw = pw * 10 + (*fmt - '0');
            fmt++;
        }
        switch (*fmt) {
        case 'c':
            bputc((char)va_arg(a, int));
            break;
        case 's': {
            const char *s = va_arg(a, const char *);
            bputs(s ? s : "(null)");
            break;
        }
        case 'd':
        case 'i': {
            long n = va_arg(a, int);
            if (n < 0) {
                bputc('-');
                n = -n;
            }
            print_u64((uint64_t)n, 10, pw, pc);
            break;
        }
        case 'u':
            print_u64(va_arg(a, uint64_t), 10, pw, pc);
            break;
        case 'x':
        case 'X':
            print_u64(va_arg(a, uint64_t), 16, pw, pc);
            break;
        case 'p': {
            bputs("0x");
            uint64_t p = (uint64_t)(uintptr_t)va_arg(a, void *);
            for (int s = 60; s >= 0; s -= 4)
                bputc("0123456789abcdef"[(p >> s) & 0xf]);
            break;
        }
        case '%':
            bputc('%');
            break;
        default:
            bputc('%');
            bputc(*fmt);
            break;
        }
        fmt++;
    }
    va_end(a);
}

/* ============================================================================
 * bootloader_main — Main bootloader entry point
 *
 * Called from boot/start.S after:
 * - CPU initialization (EL2→EL1)
 * - Stack setup
 * - BSS zeroing
 * - Data relocation
 *
 * Bootloader tasks:
 * 1. Initialize UART for debug output
 * 2. Print bootloader welcome message
 * 3. Initialize bootloader info structure
 * 4. Load kernel from storage/memory
 * 5. Jump to kernel with DTB in x0
 * ============================================================================ */
void bootloader_main(void) {
    /* x10 contains DTB address (passed from assembly) */
    uint64_t dtb_addr;
    asm volatile("mov %0, x10" : "=r"(dtb_addr));

    /* Initialize UART first so we can debug */
    uart_init(UART0_BASE, UART_CLK_HZ, UART_BAUD);

    /* Print bootloader banner */
    bootloader_printf("\r\n");
    bootloader_printf("  +------------------------------------------+\r\n");
    bootloader_printf("  |  ARMv8 AArch64 Bootloader                |\r\n");
    bootloader_printf("  |  Kernel loading bootloader               |\r\n");
    bootloader_printf("  +------------------------------------------+\r\n");
    bootloader_printf("\r\n");

    /* Print system information */
    bootloader_printf("[BOOT] UART initialized\r\n");
    bootloader_printf("[BOOT] Base: 0x%016x\r\n", UART0_BASE);
    bootloader_printf("[BOOT] Clock: %d Hz\r\n", UART_CLK_HZ);
    bootloader_printf("[BOOT] Baud: %d\r\n", UART_BAUD);
    bootloader_printf("\r\n");

    /* Print memory layout */
    uint64_t bl_start = (uint64_t)&_bootloader_start;
    uint64_t bl_end = (uint64_t)&_bootloader_end;
    bootloader_printf("[BOOT] Memory layout:\r\n");
    bootloader_printf("  bootloader       : 0x%016x -- 0x%016x (%u bytes)\r\n",
                      bl_start, bl_end, bl_end - bl_start);
    bootloader_printf("  kernel(staging)  : 0x%016x\r\n", KERNEL_IMAGE_LOAD_ADDR);
    bootloader_printf("  kernel           : 0x%016x\r\n", KERNEL_BASE);
    bootloader_printf("  DTB address      : 0x%016x\r\n", dtb_addr);
    bootloader_printf("\r\n");

    /* Initialize bootloader info with DTB address */
    bootloader_info_init(dtb_addr);

    /* Load kernel (for Approach 1: kernel is already loaded by firmware) */
    bootloader_printf("[BOOT] Loading kernel...\r\n");
    if (bootloader_load_kernel() != 0) {
        bootloader_printf("[ERROR] Failed to load kernel\r\n");
        while (1)
            ;
    }

    /* Jump to kernel (never returns) */
    bootloader_jump_to_kernel(dtb_addr);
}
