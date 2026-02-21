/* =============================================================================
 * Neutron Bootloader - Project Atom
 * include/bootloader.h  —  Bootloader API and memory layout definitions
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * This header defines the contract between bootloader and kernel:
 * - Memory addresses
 * - Bootloader information passed to kernel
 * - Function prototypes
 * =============================================================================
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------- */
#define BL_OK 0
#define BL_ERR_NOT_FOUND 1
#define BL_ERR_BAD_MAGIC 2
#define BL_ERR_TOO_LARGE 3
#define BL_ERR_BAD_CHECKSUM 4

/* ----------------------------------------------------------------
 * Kernel image header
 *
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0x00     4   Magic      "NKRN"  (0x4E4B524E)
 *   0x04     4   Version    (major<<16 | minor)
 *   0x08     4   Load addr  physical address to copy payload to
 *   0x0C     4   Entry addr physical address to jump to
 *   0x10     4   Image size payload length in bytes (after header)
 *   0x14     4   CRC32      CRC of payload bytes only
 *   0x18    40   Name       null-terminated OS name string
 *   0x40     -   Payload    raw binary (or ELF - see flags)
 * ---------------------------------------------------------------- */
#define KERNEL_MAGIC 0x4E4B524EU /* "NKRN" */
#define KERNEL_HEADER_SIZE 0x40

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t load_addr;
  uint32_t entry_addr;
  uint32_t image_size;
  uint32_t crc32;
  char name[40];
} __attribute__((packed)) kernel_header_t;

/* ----------------------------------------------------------------
 * Boot info passed to the loaded kernel
 * (placed at a well-known address so the kernel can find it)
 * ---------------------------------------------------------------- */
#define BOOT_INFO_ADDR 0x1000UL

typedef struct {
  uint32_t magic;          /* 0xB007B007               */
  uint32_t board_revision; /* from mailbox             */
  uint32_t arm_mem_size;   /* ARM-accessible RAM bytes */
  uint32_t kernel_load_addr;
  uint32_t kernel_entry_addr;
  uint32_t kernel_size;
  char bootloader_version[16];
} __attribute__((packed)) boot_info_t;

#define BOOT_INFO_MAGIC 0xB007B007U

/* ----------------------------------------------------------------
 * API
 * ---------------------------------------------------------------- */

/*
 * bl_load_kernel()
 *   Validate the kernel image at `src` (address where it was DMA'd
 *   or placed in memory), copy payload to header.load_addr, fill
 *   boot_info at BOOT_INFO_ADDR.
 *   Returns BL_OK or an error code.
 */
int bl_load_kernel(uintptr_t src, boot_info_t *out_info);

/*
 * bl_boot_kernel()
 *   Jump to entry_addr with boot_info_t * in x0.
 *   Does NOT return.
 */
void __attribute__((noreturn)) bl_boot_kernel(uintptr_t entry_addr,
                                              boot_info_t *info);

/* CRC32 helper (IEEE 802.3 polynomial) */
uint32_t crc32(const uint8_t *data, size_t len);

#endif /* BOOTLOADER_H */
