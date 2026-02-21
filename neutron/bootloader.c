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
 * =============================================================================
 */

/* ================================================================
 * neutron/bootloader.c  -  Kernel image loading & launch
 * ================================================================ */

#include "bootloader.h"
#include "platform.h"
#include "uart.h"
#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * CRC32  (IEEE 802.3 / Ethernet polynomial 0xEDB88320)
 * ---------------------------------------------------------------- */
static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320U;
      else
        crc >>= 1;
    }
    crc32_table[i] = crc;
  }
  crc32_table_ready = 1;
}

uint32_t crc32(const uint8_t *data, size_t len) {
  if (!crc32_table_ready)
    crc32_init();

  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < len; i++) {
    crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
  }
  return crc ^ 0xFFFFFFFFU;
}

/* ----------------------------------------------------------------
 * memcpy_aligned()
 *   Simple byte-by-byte copy (no libc dependency)
 * ---------------------------------------------------------------- */
static void *bl_memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--)
    *d++ = *s++;
  return dst;
}

static void *bl_memset(void *dst, int c, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  while (n--)
    *d++ = (uint8_t)c;
  return dst;
}

/* ----------------------------------------------------------------
 * bl_load_kernel()
 *
 *  src  - pointer to the start of the kernel image in memory.
 *         (In QEMU we simulate this by placing a pre-built kernel
 *          binary at KERNEL_LOAD_ADDR before booting, or the
 *          bootloader can receive it over UART - see bl_recv_uart.)
 *
 *  Flow:
 *    1. Read and validate the kernel_header_t at `src`
 *    2. Verify CRC32 of the payload
 *    3. Copy payload to header.load_addr
 *    4. Fill boot_info_t at BOOT_INFO_ADDR
 * ---------------------------------------------------------------- */
int bl_load_kernel(uintptr_t src, boot_info_t *out_info) {
  const kernel_header_t *hdr = (const kernel_header_t *)src;

  uart_printf("[BL] Examining image at %p\n", (void *)src);

  /* 1. Magic check */
  if (hdr->magic != KERNEL_MAGIC) {
    uart_printf("[BL] ERROR: bad magic %X (expected %X)\n", hdr->magic,
                KERNEL_MAGIC);
    return BL_ERR_BAD_MAGIC;
  }

  uart_printf("[BL] Kernel name    : %s\n", hdr->name);
  uart_printf("[BL] Version        : %d.%d\n", (hdr->version >> 16) & 0xFFFF,
              hdr->version & 0xFFFF);
  uart_printf("[BL] Load address   : %X\n", hdr->load_addr);
  uart_printf("[BL] Entry address  : %X\n", hdr->entry_addr);
  uart_printf("[BL] Payload size   : %u bytes\n", hdr->image_size);

  /* 2. Size sanity */
  if (hdr->image_size == 0 || hdr->image_size > KERNEL_MAX_SIZE) {
    uart_printf("[BL] ERROR: image size %u out of range\n", hdr->image_size);
    return BL_ERR_TOO_LARGE;
  }

  /* 3. CRC32 verification */
  const uint8_t *payload = (const uint8_t *)(src + KERNEL_HEADER_SIZE);
  uint32_t computed_crc = crc32(payload, hdr->image_size);

  uart_printf("[BL] CRC32 expected : %X\n", hdr->crc32);
  uart_printf("[BL] CRC32 computed : %X\n", computed_crc);

  if (computed_crc != hdr->crc32) {
    uart_printf("[BL] ERROR: CRC32 mismatch - image corrupt!\n");
    return BL_ERR_BAD_CHECKSUM;
  }
  uart_printf("[BL] CRC32 OK\n");

  /* 4. Copy payload to its final load address */
  uart_printf("[BL] Copying %u bytes to %X ...\n", hdr->image_size,
              hdr->load_addr);
  bl_memcpy((void *)(uintptr_t)hdr->load_addr, payload, hdr->image_size);
  uart_printf("[BL] Copy done\n");

  /* 5. Fill boot_info */
  boot_info_t *info = (boot_info_t *)(uintptr_t)BOOT_INFO_ADDR;
  bl_memset(info, 0, sizeof(*info));
  info->magic = BOOT_INFO_MAGIC;
  info->kernel_load_addr = hdr->load_addr;
  info->kernel_entry_addr = hdr->entry_addr;
  info->kernel_size = hdr->image_size;

  /* Copy version string */
  const char ver[] = "Neutron-1.0";
  bl_memcpy(info->bootloader_version, ver, sizeof(ver));

  if (out_info)
    bl_memcpy(out_info, info, sizeof(*info));

  return BL_OK;
}

/* ----------------------------------------------------------------
 * bl_boot_kernel()
 *   Flush caches (ISB/DSB), then jump.
 *   Convention: x0 = pointer to boot_info_t
 * ---------------------------------------------------------------- */
void __attribute__((noreturn)) bl_boot_kernel(uintptr_t entry_addr,
                                              boot_info_t *info) {
  uart_printf("[BL] Jumping to kernel at %p\n", (void *)entry_addr);

  /* Ensure all writes are visible before the jump */
  __asm__ volatile("dsb sy\n"
                   "isb\n" ::
                       : "memory");

  /* Call kernel: x0 = boot_info_t * */
  void (*kernel_entry)(boot_info_t *) = (void (*)(boot_info_t *))entry_addr;
  kernel_entry(info);

  /* Should never reach here */
  while (1)
    __asm__ volatile("wfe");
  __builtin_unreachable();
}
