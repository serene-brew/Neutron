/* ================================================================
 * Neutron Bootloader - Project Atom
 * include/platform.h  -  BCM2710 / RPi Zero 2W memory map
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * QEMU machine : raspi3b  (same peripheral base as BCM2837/BCM2710)
 * ================================================================ */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "neutron.h"

/* ----------------------------------------------------------------
 * Peripheral base addresses
 * ---------------------------------------------------------------- */
/* BCM2835 SDHOST - QEMU raspi3b wires the SD card here (not Arasan EMMC) */
#define SDHOST_BASE (MMIO_BASE + 0x00202000UL)

/* Mailbox */
#define MBOX_BASE (MMIO_BASE + 0x0000B880UL)

/* ----------------------------------------------------------------
 * Memory layout
 * ---------------------------------------------------------------- */
#define BOOTLOADER_LOAD_ADDR 0x80000UL    /* GPU drops kernel8.img here */
#define KERNEL_LOAD_ADDR 0x100000UL       /* Where we place kernel.bin  */
#define KERNEL_MAX_SIZE (4 * 1024 * 1024) /* 4 MiB                */

/* Stack lives just below the bootloader image */
#define STACK_TOP BOOTLOADER_LOAD_ADDR

/* ----------------------------------------------------------------
 * Mailbox register offsets
 * ---------------------------------------------------------------- */
#define MBOX_READ 0x00
#define MBOX_STATUS 0x18
#define MBOX_WRITE 0x20
#define MBOX_FULL 0x80000000
#define MBOX_EMPTY 0x40000000

/* Mailbox channels */
#define MBOX_CH_PROP 8 /* Property tags channel       */

/* Mailbox tags */
#define MBOX_TAG_GET_BOARD_REVISION 0x00010002
#define MBOX_TAG_GET_ARM_MEM 0x00010005
#define MBOX_TAG_LAST 0x00000000

#endif /* PLATFORM_H */
