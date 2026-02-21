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

/* ----------------------------------------------------------------
 * Peripheral base addresses
 * ---------------------------------------------------------------- */
#define MMIO_BASE 0x3F000000UL /* BCM2837 peripherals  */

/* GPIO */
#define GPIO_BASE (MMIO_BASE + 0x00200000UL)

/* PL011 UART0  (used by QEMU raspi3b for -serial stdio) */
#define UART0_BASE (MMIO_BASE + 0x00201000UL)

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
 * PL011 UART register offsets
 * ---------------------------------------------------------------- */
#define UART_DR 0x00   /* Data register               */
#define UART_FR 0x18   /* Flag register               */
#define UART_IBRD 0x24 /* Integer baud rate divisor   */
#define UART_FBRD 0x28 /* Fractional baud rate divisor*/
#define UART_LCRH 0x2C /* Line control register       */
#define UART_CR 0x30   /* Control register            */
#define UART_IMSC 0x38 /* Interrupt mask set/clear    */
#define UART_ICR 0x44  /* Interrupt clear register    */

/* FR bits */
#define UART_FR_RXFE (1 << 4) /* Receive  FIFO empty       */
#define UART_FR_TXFF (1 << 5) /* Transmit FIFO full        */
#define UART_FR_BUSY (1 << 3) /* UART busy                 */

/* LCRH bits */
#define UART_LCRH_FEN (1 << 4)      /* FIFO enable               */
#define UART_LCRH_WLEN_8 (0x3 << 5) /* 8-bit word length         */

/* CR bits */
#define UART_CR_UARTEN (1 << 0) /* UART enable               */
#define UART_CR_TXE (1 << 8)    /* TX enable                 */
#define UART_CR_RXE (1 << 9)    /* RX enable                 */

/* ----------------------------------------------------------------
 * GPIO register offsets
 * ---------------------------------------------------------------- */
#define GPFSEL1 0x04   /* GPIO Function Select 1      */
#define GPPUD 0x94     /* GPIO Pin Pull-up/down       */
#define GPPUDCLK0 0x98 /* GPIO PUD Clock reg 0        */

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
