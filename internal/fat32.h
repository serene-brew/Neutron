/* ================================================================
 * Neutron Bootloader - Project Atom
 * include/fat32.h  -  Read-only FAT32 driver interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Supports:
 *   - MBR partition table (reads partition 0)
 *   - FAT32 volume (BPB / EBPB parsing)
 *   - 8.3 short filenames (case-insensitive)
 *   - Cluster chain following via FAT
 *   - Multi-cluster file reads into a flat buffer
 * ================================================================ */

#ifndef FAT32_H
#define FAT32_H

#include <stddef.h>
#include <stdint.h>

/* Return codes */
#define FAT32_OK 0
#define FAT32_ERR_MOUNT 1
#define FAT32_ERR_NOT_FAT32 2
#define FAT32_ERR_NOT_FOUND 3
#define FAT32_ERR_IO 4
#define FAT32_ERR_TOOLARGE 5

/* BPB structure (simplified from raspi3-tutorial) */
typedef struct {
  uint8_t jmp[3];
  char oem[8];
  uint8_t bps0;  /* bytes per sector low */
  uint8_t bps1;  /* bytes per sector high */
  uint8_t spc;   /* sectors per cluster */
  uint16_t rsc;  /* reserved sector count */
  uint8_t nf;    /* number of FATs */
  uint8_t nr0;   /* number of root entries low */
  uint8_t nr1;   /* number of root entries high */
  uint16_t ts16; /* total sectors 16-bit */
  uint8_t media;
  uint16_t spf16; /* sectors per FAT 16-bit */
  uint16_t spt;   /* sectors per track */
  uint16_t nh;    /* number of heads */
  uint32_t hs;    /* hidden sectors */
  uint32_t ts32;  /* total sectors 32-bit */
  uint32_t spf32; /* sectors per FAT 32-bit */
  uint32_t flg;   /* flags */
  uint32_t rc;    /* root cluster */
  char vol[6];
  char fst[8]; /* filesystem type "FAT32   " */
  char dmy[20];
  char fst2[8]; /* filesystem type 2 */
} __attribute__((packed)) bpb_t;

/* Directory entry structure - must be exactly 32 bytes */
typedef struct {
  char name[8];        /* 0-7: filename */
  char ext[3];         /* 8-10: extension */
  uint8_t attr;        /* 11: attribute */
  uint8_t reserved[8]; /* 12-19: reserved/creation time */
  uint16_t ch;         /* 20-21: cluster high */
  uint16_t wtime;      /* 22-23: write time */
  uint16_t wdate;      /* 24-25: write date */
  uint16_t cl;         /* 26-27: cluster low */
  uint32_t size;       /* 28-31: file size */
} __attribute__((packed)) fatdir_t;

/* Maximum file size we will load (must fit destination buffer) */
#define FAT32_MAX_FILE_SIZE (8 * 1024 * 1024) /* 8 MiB */

/*
 * fat32_mount()
 *   Read MBR, locate partition 0, parse FAT32 BPB.
 *   Must be called once after sdcard_init().
 *   Returns FAT32_OK or an error code.
 */
int fat32_mount(void);

/*
 * fat32_read_file()
 *   Find `filename` (8.3, case-insensitive) in the root directory,
 *   read its entire contents into `dest`.
 *   `dest_size` is the maximum bytes to write.
 *   `bytes_read` receives the actual file size if non-NULL.
 *   Returns FAT32_OK or an error code.
 */
int fat32_read_file(const char *filename, void *dest, uint32_t dest_size,
                    uint32_t *bytes_read);

#endif /* FAT32_H */
