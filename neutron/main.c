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
#include "fat32.h"
#include "mbox.h"
#include "platform.h"
#include "sdcard.h"
#include "uart.h"
#include <stdint.h>

/* ----------------------------------------------------------------
 * ANSI colour helpers
 * ---------------------------------------------------------------- */
#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_RED "\x1b[31m"

/* ----------------------------------------------------------------
 * Banner
 * ---------------------------------------------------------------- */
static void print_banner(void) {
  uart_puts(ANSI_BOLD ANSI_CYAN);
  uart_puts("\n");

  uart_puts(ANSI_RESET);
  uart_puts(ANSI_GREEN);
  uart_puts("       ~ Neutron Bootloader  v1.0.1\n");
  uart_puts(ANSI_RESET);
  uart_puts(
      "------------------------------------------------------------------\n");
}

/* ----------------------------------------------------------------
 * read_exception_level()
 * ---------------------------------------------------------------- */
static uint32_t read_exception_level(void) {
  uint64_t el;
  __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
  return (uint32_t)((el >> 2) & 0x3);
}

/* ----------------------------------------------------------------
 * read_mpidr()
 * ---------------------------------------------------------------- */
static uint64_t read_mpidr(void) {
  uint64_t mpidr;
  __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
  return mpidr;
}

/* ----------------------------------------------------------------
 * neutron_main()  -  called from start.S
 * ---------------------------------------------------------------- */
void neutron_main(void) {
  /* ----- Hardware init ----- */
  uart_init();
  print_banner();

  /* ----- CPU state ----- */
  uint32_t el = read_exception_level();
  uint64_t mpidr = read_mpidr();

  uart_printf(ANSI_BOLD "[CPU] Exception Level : EL%d\n" ANSI_RESET, el);
  uart_printf("[CPU] MPIDR           : %x\n", mpidr);
  uart_printf("[CPU] Core ID         : %d\n", (int)(mpidr & 0xFF));

  /* ----- Board info via mailbox ----- */
  uart_puts("\n[MBOX] Querying board information...\n");
  uint32_t board_rev = mbox_get_board_revision();
  uint32_t arm_mem = mbox_get_arm_mem_size();

  uart_printf("[MBOX] Board revision : %X\n", board_rev);
  uart_printf("[MBOX] ARM memory     : %u MiB\n", arm_mem >> 20);

  /* ----- Identify board variant ----- */
  uart_puts("\n[BL] Board identification:\n");
  if (board_rev != 0) {
    uart_printf("[BL]   Revision code  : %X\n", board_rev);
    if ((board_rev & 0xFFFFFF) == 0x902120 ||
        (board_rev & 0xFF0000) == 0x900000) {
      uart_puts("[BL]   Board          : Raspberry Pi Zero 2W\n");
    } else {
      uart_puts("[BL]   Board          : Raspberry Pi (generic)\n");
    }
  } else {
    uart_puts("[BL]   Board          : QEMU simulated (raspi3b)\n");
  }

  /* ----- Initialise SD card ----- */
  uart_puts("\n[BL] Initialising SD card...\n");
  int rc = sdcard_init();
  if (rc != SD_OK) {
    uart_printf(
        ANSI_RED "[BL] FATAL: SD card init failed (error %d)\n" ANSI_RESET, rc);
    uart_puts("[BL] System halted.\n");
    while (1)
      __asm__ volatile("wfe");
  }

  /* ----- Mount FAT32 volume ----- */
  uart_puts("\n[BL] Mounting FAT32 volume...\n");
  rc = fat32_mount();
  if (rc != FAT32_OK) {
    uart_printf(
        ANSI_RED "[BL] FATAL: FAT32 mount failed (error %d)\n" ANSI_RESET, rc);
    uart_puts("[BL] System halted.\n");
    while (1)
      __asm__ volatile("wfe");
  }

  /* ----- Load kernel.bin from SD card into staging area ----- */
  uart_puts("\n[BL] Loading kernel.bin from SD card...\n");

  uint32_t bytes_loaded = 0;
  rc = fat32_read_file("ATOM.BIN", (void *)(uintptr_t)KERNEL_LOAD_ADDR,
                       KERNEL_MAX_SIZE, &bytes_loaded);

  if (rc != FAT32_OK) {
    uart_printf(ANSI_RED
                "[BL] FATAL: atom.bin not found on SD card (error %d)\n"
                "[BL]        Ensure kernel.bin is in the FAT32 root "
                "directory.\n" ANSI_RESET,
                rc);
    uart_puts("[BL] System halted.\n");
    while (1)
      __asm__ volatile("wfe");
  }

  uart_printf(ANSI_YELLOW "[BL] atom.bin loaded: %u bytes at 0x%X\n" ANSI_RESET,
              bytes_loaded, (uint32_t)KERNEL_LOAD_ADDR);

  /* ----- Validate NKRN magic ----- */
  const uint32_t *probe = (const uint32_t *)(uintptr_t)KERNEL_LOAD_ADDR;
  if (*probe != KERNEL_MAGIC) {
    uart_printf(
        ANSI_RED
        "[BL] FATAL: bad magic at 0x%X - got 0x%X, expected 0x%X\n"
        "[BL]        Is kernel.bin packed with pack_kernel.py?\n" ANSI_RESET,
        (uint32_t)KERNEL_LOAD_ADDR, *probe, KERNEL_MAGIC);
    uart_puts("[BL] System halted.\n");
    while (1)
      __asm__ volatile("wfe");
  }

  /* ----- Run bootloader validation + copy ----- */
  uart_puts("\n[BL] Validating and loading kernel image...\n");

  boot_info_t boot_info;
  rc = bl_load_kernel(KERNEL_LOAD_ADDR, &boot_info);

  if (rc != BL_OK) {
    uart_printf(ANSI_RED
                "[BL] FATAL: kernel validation failed (error %d)\n" ANSI_RESET,
                rc);
    uart_puts("[BL] System halted.\n");
    while (1)
      __asm__ volatile("wfe");
  }

  /* Fill in mailbox-obtained fields */
  boot_info.board_revision = board_rev;
  boot_info.arm_mem_size = arm_mem;

  /* ----- Boot countdown ----- */
  uart_puts("\n[BL] Kernel loaded successfully.\n");
  uart_printf("[BL] Entry point : %p\n",
              (void *)(uintptr_t)boot_info.kernel_entry_addr);

  for (int i = 3; i > 0; i--) {
    for (volatile uint32_t d = 0; d < 2000000U; d++)
      __asm__ volatile("nop");
  }

  bl_boot_kernel((uintptr_t)boot_info.kernel_entry_addr, &boot_info);

  __builtin_unreachable();
}
