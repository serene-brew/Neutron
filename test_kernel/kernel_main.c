/* =============================================================================
 * Neutron Bootloader - Project Atom
 * test_kernel/kernel_main.c  —  Minimal test kernel for bootloader testing
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Purpose: verify that the Neutron bootloader correctly validates,
 * loads, and hands off to a real kernel binary.
 *
 * This kernel:
 *   1. Initialises PL011 UART0 independently (can't rely on the
 *      bootloader having left it in a known state after the jump)
 *   2. Prints a banner and the boot_info_t fields passed by the BL
 *   3. Blinks an "alive" pattern via UART and spins forever
 *
 * Received from bootloader (in x0 / first argument):
 *   pointer to boot_info_t at physical address 0x1000
 * ================================================================ */

#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * Peripheral base  (BCM2837 / QEMU raspi3b)
 * ---------------------------------------------------------------- */
#define MMIO_BASE 0x3F000000UL
#define GPIO_BASE (MMIO_BASE + 0x00200000UL)
#define UART0_BASE (MMIO_BASE + 0x00201000UL)

/* PL011 register offsets */
#define UART_DR 0x00
#define UART_FR 0x18
#define UART_IBRD 0x24
#define UART_FBRD 0x28
#define UART_LCRH 0x2C
#define UART_CR 0x30
#define UART_IMSC 0x38
#define UART_ICR 0x44

#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)
#define UART_LCRH_FEN (1 << 4)
#define UART_LCRH_8BIT (0x3 << 5)
#define UART_CR_EN (1 << 0)
#define UART_CR_TXE (1 << 8)
#define UART_CR_RXE (1 << 9)

/* GPIO */
#define GPFSEL1 0x04
#define GPPUD 0x94
#define GPPUDCLK0 0x98

/* ----------------------------------------------------------------
 * boot_info_t  -  must match bootloader's bootloader.h exactly
 * ---------------------------------------------------------------- */
#define BOOT_INFO_MAGIC 0xB007B007U

typedef struct {
  uint32_t magic;
  uint32_t board_revision;
  uint32_t arm_mem_size;
  uint32_t kernel_load_addr;
  uint32_t kernel_entry_addr;
  uint32_t kernel_size;
  char bootloader_version[16];
} __attribute__((packed)) boot_info_t;

/* ----------------------------------------------------------------
 * MMIO helpers
 * ---------------------------------------------------------------- */
static inline void wr(uintptr_t addr, uint32_t v) {
  *(volatile uint32_t *)addr = v;
}
static inline uint32_t rd(uintptr_t addr) { return *(volatile uint32_t *)addr; }
static void delay(uint32_t n) {
  while (n--)
    __asm__ volatile("nop");
}

/* ----------------------------------------------------------------
 * uart_init()  -  bring up PL011 fresh at 115200 8N1
 * ---------------------------------------------------------------- */
static void uart_init(void) {
  wr(UART0_BASE + UART_CR, 0);

  /* GPIO 14/15 → ALT0 */
  uint32_t sel = rd(GPIO_BASE + GPFSEL1);
  sel &= ~(7u << 12 | 7u << 15); /* clear pins 14, 15 */
  sel |= (4u << 12 | 4u << 15);  /* ALT0 = 4 */
  wr(GPIO_BASE + GPFSEL1, sel);
  wr(GPIO_BASE + GPPUD, 0);
  delay(150);
  wr(GPIO_BASE + GPPUDCLK0, (1u << 14) | (1u << 15));
  delay(150);
  wr(GPIO_BASE + GPPUDCLK0, 0);

  wr(UART0_BASE + UART_ICR, 0x7FF);
  wr(UART0_BASE + UART_IBRD, 26);
  wr(UART0_BASE + UART_FBRD, 3);
  wr(UART0_BASE + UART_LCRH, UART_LCRH_8BIT | UART_LCRH_FEN);
  wr(UART0_BASE + UART_IMSC, 0);
  wr(UART0_BASE + UART_CR, UART_CR_EN | UART_CR_TXE | UART_CR_RXE);
}

static void putc(char c) {
  if (c == '\n')
    putc('\r');
  while (rd(UART0_BASE + UART_FR) & UART_FR_TXFF)
    ;
  wr(UART0_BASE + UART_DR, (uint32_t)(unsigned char)c);
}

static void puts(const char *s) {
  while (*s)
    putc(*s++);
}

static void puthex32(uint32_t v) {
  puts("0x");
  const char h[] = "0123456789ABCDEF";
  for (int i = 28; i >= 0; i -= 4)
    putc(h[(v >> i) & 0xF]);
}

static void putdec(uint32_t v) {
  if (v == 0) {
    putc('0');
    return;
  }
  char buf[10];
  int i = 0;
  while (v) {
    buf[i++] = '0' + (v % 10);
    v /= 10;
  }
  for (int j = i - 1; j >= 0; j--)
    putc(buf[j]);
}

/* ----------------------------------------------------------------
 * kernel_main()  -  called from kernel_start.S with x0=boot_info*
 * ---------------------------------------------------------------- */
void kernel_main(boot_info_t *info) {
  uart_init();

  puts("\n");
  puts("  +----------------------------------------------+\n");
  puts("  |        Neutron ATOM Test Kernel  v1.0        |\n");
  puts("  |        AArch64 / Raspberry Pi Zero 2W        |\n");
  puts("  +----------------------------------------------+\n");
  puts("\n");

  /* ---- Verify boot_info magic ---- */
  puts("[KERNEL] Checking boot_info... ");
  if (info != (void *)0 && info->magic == BOOT_INFO_MAGIC) {
    puts("OK\n");
  } else {
    puts("MISSING (booted without Neutron)\n");
    /* carry on anyway - still useful to see the kernel alive */
    goto alive_loop;
  }

  /* ---- Print boot_info fields ---- */
  puts("[KERNEL] Boot info from Neutron bootloader:\n");

  puts("[KERNEL]   Magic            : ");
  puthex32(info->magic);
  puts("\n");

  puts("[KERNEL]   Board revision   : ");
  puthex32(info->board_revision);
  puts("\n");

  puts("[KERNEL]   ARM memory       : ");
  putdec(info->arm_mem_size >> 20);
  puts(" MiB\n");

  puts("[KERNEL]   Kernel load addr : ");
  puthex32(info->kernel_load_addr);
  puts("\n");

  puts("[KERNEL]   Kernel entry     : ");
  puthex32(info->kernel_entry_addr);
  puts("\n");

  puts("[KERNEL]   Kernel size      : ");
  putdec(info->kernel_size);
  puts(" bytes\n");

  puts("[KERNEL]   Bootloader ver   : ");
  puts(info->bootloader_version);
  puts("\n");

alive_loop:
  /* Heartbeat: print a dot every ~500 ms */
  uint32_t tick = 0;
  while (1) {
    for (volatile uint32_t d = 0; d < 5000000U; d++)
      __asm__ volatile("nop");
    putc('.');
    tick++;
    if (tick % 40 == 0)
      putc('\n');
  }
}
