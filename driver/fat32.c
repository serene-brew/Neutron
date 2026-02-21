/* =============================================================================
 * Neutron Bootloader - Project Atom
 * driver/fat32.c  -  Read-only FAT32 driver
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Reads MBR -> VBR -> FAT -> Root directory -> File clusters.
 * ================================================================ */

#include "fat32.h"
#include "sdcard.h"
#include "uart.h"
#include <stddef.h>
#include <stdint.h>

/* Sector buffer for temporary reads */
static uint8_t sector_buf[512];

/* Partition LBA (set during mount) */
static uint32_t partitionlba = 0;

/* Convert char to uppercase */
static char to_upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/* Compare 8.3 filename - case insensitive */
static int name_matches(const fatdir_t *e, const char *filename) {
  char name83[13];
  int idx = 0;

  /* Build 8.3 name from entry */
  for (int i = 0; i < 8; i++) {
    char c = e->name[i];
    if (c == ' ')
      break;
    name83[idx++] = to_upper(c);
  }
  if (e->ext[0] != ' ') {
    name83[idx++] = '.';
    for (int i = 0; i < 3; i++) {
      char c = e->ext[i];
      if (c == ' ')
        break;
      name83[idx++] = to_upper(c);
    }
  }
  name83[idx] = '\0';

  /* Compare */
  const char *f = filename;
  int i = 0;
  while (name83[i] && to_upper(*f) == name83[i]) {
    i++;
    f++;
  }
  return (name83[i] == '\0' && *f == '\0');
}

/* ----------------------------------------------------------------
 * fat32_mount()
 * ---------------------------------------------------------------- */
int fat32_mount(void) {
  bpb_t *bpb = (bpb_t *)sector_buf;

  /* Read MBR (sector 0) */
  uart_puts("[FAT] Reading MBR (sector 0)...\n");
  int ret = sdcard_read_block(0, sector_buf);
  uart_puts("[FAT] sdcard_read_block returned: ");
  uart_puthex32(ret);
  uart_puts("\n");
  if (ret != SD_OK) {
    uart_puts("[FAT] ERROR: cannot read MBR\n");
    return FAT32_ERR_IO;
  }
  uart_puts("[FAT] MBR read OK, checking magic...\n");

  /* Check magic */
  uart_puts("[FAT] Magic bytes: ");
  uart_puthex32(sector_buf[510]);
  uart_puts(" ");
  uart_puthex32(sector_buf[511]);
  uart_puts("\n");
  if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
    uart_printf("[FAT] ERROR: Bad magic in MBR: %02X %02X\n", sector_buf[510],
                sector_buf[511]);
    return FAT32_ERR_MOUNT;
  }
  uart_puts("[FAT] Magic OK (0x55 0xAA)\n");

  /* Check partition type - must be FAT16 LBA (0x0E) or FAT32 LBA (0x0C) */
  uart_puts("[FAT] Checking partition type...\n");
  uart_puts("[FAT] Partition type byte: ");
  uart_puthex32(sector_buf[0x1C2]);
  uart_puts("\n");
  if (sector_buf[0x1C2] != 0x0E && sector_buf[0x1C2] != 0x0C) {
    uart_printf(
        "[FAT] ERROR: Wrong partition type: 0x%02X (expected 0x0E or 0x0C)\n",
        sector_buf[0x1C2]);
    return FAT32_ERR_MOUNT;
  }
  uart_puts("[FAT] Partition type OK\n");

  /* Get partition LBA */
  uint8_t b0 = sector_buf[0x1C6];
  uint8_t b1 = sector_buf[0x1C7];
  uint8_t b2 = sector_buf[0x1C8];
  uart_puts("[FAT] Calculating partition LBA...\n");
  uint8_t b3 = sector_buf[0x1C9];
  partitionlba = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  uart_puts("[FAT] LBA=");
  uart_puthex32(partitionlba);
  uart_puts("\n");

  /* Read boot record (VBR) */
  uart_puts("[FAT] Reading VBR at partition LBA...\n");
  ret = sdcard_read_block(partitionlba, sector_buf);
  uart_puts("[FAT] VBR read returned: ");
  uart_puthex32(ret);
  uart_puts("\n");
  if (ret != SD_OK) {
    uart_puts("[FAT] ERROR: Unable to read boot record\n");
    return FAT32_ERR_IO;
  }
  uart_puts("[FAT] VBR read OK, checking FAT magic...\n");

  /* Check filesystem type - look for "FAT" magic */
  if (!(bpb->fst[0] == 'F' && bpb->fst[1] == 'A' && bpb->fst[2] == 'T') &&
      !(bpb->fst2[0] == 'F' && bpb->fst2[1] == 'A' && bpb->fst2[2] == 'T')) {
    uart_puts("[FAT] ERROR: Unknown file system type\n");
    return FAT32_ERR_NOT_FAT32;
  }

  uart_puts("[FAT] FAT type: ");
  if (bpb->spf16 > 0) {
    uart_puts("FAT16\n");
    return FAT32_ERR_NOT_FAT32;
  } else {
    uart_puts("FAT32\n");
  }

  uart_puts("[FAT] Reading BPB values...\n");
  /* Calculate key values */
  uint32_t bytes_per_sector = bpb->bps0 + (bpb->bps1 << 8);
  uint32_t sectors_per_cluster = bpb->spc;
  uint32_t fat_size = bpb->spf32;
  uint32_t root_cluster = bpb->rc;
  (void)bytes_per_sector;
  (void)sectors_per_cluster;
  (void)fat_size;
  (void)root_cluster;

  uart_puts("[FAT] BPB values OK\n");
  uart_puts("[FAT] Mount complete\n");
  return FAT32_OK;
}

/* ----------------------------------------------------------------
 * fat32_read_file()
 * ---------------------------------------------------------------- */
int fat32_read_file(const char *filename, void *dest, uint32_t dest_size,
                    uint32_t *bytes_read) {
  bpb_t *bpb = (bpb_t *)sector_buf;
  fatdir_t *dir;
  uint32_t root_sec, fat_sec, data_sec, cluster_size;
  uint32_t cluster, file_cluster = 0, file_size = 0;
  uint8_t *out = (uint8_t *)dest;
  uint32_t remaining;
  int found = 0;

  if (!partitionlba) {
    uart_puts("[FAT] ERROR: not mounted\n");
    return FAT32_ERR_MOUNT;
  }

  uart_printf("[FAT] Searching for \"%s\"...\n", filename);

  /* Re-read VBR to get BPB data */
  if (sdcard_read_block(partitionlba, sector_buf) != SD_OK) {
    return FAT32_ERR_IO;
  }

  /* Calculate sector locations */
  uint32_t fat_size = bpb->spf32;
  uint32_t num_fats = bpb->nf;
  uint32_t reserved_sectors = bpb->rsc;
  uint32_t sectors_per_cluster = bpb->spc;
  uint32_t root_cluster = bpb->rc;

  /* FAT region starts after reserved sectors */
  fat_sec = partitionlba + reserved_sectors;
  /* Data region starts after FAT(s) */
  data_sec = fat_sec + (num_fats * fat_size);
  cluster_size = sectors_per_cluster * 512;

  /* Find root directory - for FAT32, root is at cluster rc */
  root_sec = data_sec + (root_cluster - 2) * sectors_per_cluster;

  uart_puts("[FAT] Root sec=");
  uart_puthex32(root_sec);
  uart_puts(" fat_sec=");
  uart_puthex32(fat_sec);
  uart_puts(" data_sec=");
  uart_puthex32(data_sec);
  uart_puts("\n");
  uart_puts("[FAT] root_cluster=");
  uart_puthex32(root_cluster);
  uart_puts(" sectors_per_cluster=");
  uart_puthex32(sectors_per_cluster);
  uart_puts("\n");

  /* Load root directory */
  if (sdcard_read_block(root_sec, sector_buf) != SD_OK) {
    uart_puts("[FAT] ERROR: Unable to load root directory\n");
    return FAT32_ERR_IO;
  }
  dir = (fatdir_t *)sector_buf;

  uart_puts("[FAT] Searching directory entries...\n");
  int entry_count = 0;

  /* Search for file */
  for (; dir->name[0] != 0 && !found; dir++) {
    entry_count++;

    /* Print first byte for debug */
    if (entry_count <= 5) {
      uart_puts("[FAT] Entry #");
      uart_puthex32(entry_count);
      uart_puts(" first byte=");
      uart_puthex32(dir->name[0]);
      uart_puts(" attr=");
      uart_puthex32(dir->attr);
      uart_puts("\n");
    }

    /* Skip deleted entries */
    if ((uint8_t)dir->name[0] == 0xE5)
      continue;
    /* Skip long name entries */
    if (dir->attr == 0x0F)
      continue;
    /* Skip directories and volume labels */
    if (dir->attr & 0x10)
      continue; /* ATTR_DIRECTORY */
    if (dir->attr & 0x08)
      continue; /* ATTR_VOLUME_ID */

    /* Print entry name for debug */
    uart_puts("[FAT] Checking entry: '");
    for (int i = 0; i < 8 && dir->name[i] != ' '; i++) {
      uart_putc(dir->name[i]);
    }
    uart_puts("'\n");

    if (name_matches(dir, filename)) {
      file_cluster = ((uint32_t)dir->ch << 16) | dir->cl;
      file_size = dir->size;
      found = 1;
      uart_puts("[FAT] Found match!\n");
    }
  }

  uart_puts("[FAT] Checked ");
  uart_puthex32(entry_count);
  uart_puts(" entries\n");

  if (!found) {
    uart_printf("[FAT] File \"%s\" not found\n", filename);
    return FAT32_ERR_NOT_FOUND;
  }

  uart_printf("[FAT] Found \"%s\": cluster=%u size=%u bytes\n", filename,
              file_cluster, file_size);

  /* Size check */
  if (file_size > dest_size) {
    uart_printf("[FAT] ERROR: file too large (%u > %u)\n", file_size,
                dest_size);
    return FAT32_ERR_TOOLARGE;
  }

  /* Read file data, following cluster chain */
  cluster = file_cluster;
  remaining = file_size;

  while (remaining > 0) {
    uint32_t cluster_lba = data_sec + (cluster - 2) * sectors_per_cluster;
    uint32_t bytes_this_cluster =
        (remaining < cluster_size) ? remaining : cluster_size;
    uint32_t sectors_to_read = (bytes_this_cluster + 511) / 512;

    /* Read sectors in this cluster */
    for (uint32_t s = 0; s < sectors_to_read; s++) {
      if (sdcard_read_block(cluster_lba + s, sector_buf) != SD_OK) {
        return FAT32_ERR_IO;
      }

      uint32_t to_copy = (remaining < 512) ? remaining : 512;
      for (uint32_t b = 0; b < to_copy; b++) {
        *out++ = sector_buf[b];
      }
      remaining -= to_copy;
      if (remaining == 0)
        break;
    }

    if (remaining == 0)
      break;

    /* Read FAT to find next cluster */
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_sec + fat_offset / 512;
    uint32_t fat_index = (fat_offset % 512) / 4;

    if (sdcard_read_block(fat_sector, sector_buf) != SD_OK) {
      return FAT32_ERR_IO;
    }

    cluster = ((uint32_t *)sector_buf)[fat_index] & 0x0FFFFFFF;

    /* Check for end of chain or bad cluster */
    if (cluster >= 0x0FFFFFF8 || cluster == 0x0FFFFFF7) {
      break;
    }
  }

  if (bytes_read) {
    *bytes_read = file_size - remaining;
  }

  uart_printf("[FAT] Read %u bytes OK\n", file_size - remaining);
  return FAT32_OK;
}
