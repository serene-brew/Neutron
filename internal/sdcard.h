/* ================================================================
 * Neutron Bootloader - Project Atom
 * include/sdcard.h  -  BCM2710 EMMC / SDHCI driver interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * SDHCI host controller base: 0x3F300000 (MMIO_BASE + 0x300000)
 * Supports SDSC / SDHC / SDXC cards.
 * ================================================================ */

#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>

/* Return codes */
#define SD_OK 0
#define SD_ERR_TIMEOUT 1
#define SD_ERR_CMD 2
#define SD_ERR_DATA 3
#define SD_ERR_RESET 4
#define SD_ERR_VOLTAGE 5
#define SD_ERR_INIT 6

/* Register pointers (volatile for MMIO) */
#define EMMC_ARG2 ((volatile uint32_t *)(MMIO_BASE + 0x00300000))
#define EMMC_BLKSIZECNT ((volatile uint32_t *)(MMIO_BASE + 0x00300004))
#define EMMC_ARG1 ((volatile uint32_t *)(MMIO_BASE + 0x00300008))
#define EMMC_CMDTM ((volatile uint32_t *)(MMIO_BASE + 0x0030000C))
#define EMMC_RESP0 ((volatile uint32_t *)(MMIO_BASE + 0x00300010))
#define EMMC_RESP1 ((volatile uint32_t *)(MMIO_BASE + 0x00300014))
#define EMMC_RESP2 ((volatile uint32_t *)(MMIO_BASE + 0x00300018))
#define EMMC_RESP3 ((volatile uint32_t *)(MMIO_BASE + 0x0030001C))
#define EMMC_DATA ((volatile uint32_t *)(MMIO_BASE + 0x00300020))
#define EMMC_STATUS ((volatile uint32_t *)(MMIO_BASE + 0x00300024))
#define EMMC_CONTROL0 ((volatile uint32_t *)(MMIO_BASE + 0x00300028))
#define EMMC_CONTROL1 ((volatile uint32_t *)(MMIO_BASE + 0x0030002C))
#define EMMC_INTERRUPT ((volatile uint32_t *)(MMIO_BASE + 0x00300030))
#define EMMC_INT_MASK ((volatile uint32_t *)(MMIO_BASE + 0x00300034))
#define EMMC_INT_EN ((volatile uint32_t *)(MMIO_BASE + 0x00300038))
#define EMMC_CONTROL2 ((volatile uint32_t *)(MMIO_BASE + 0x0030003C))
#define EMMC_SLOTISR_VER ((volatile uint32_t *)(MMIO_BASE + 0x003000FC))

/* Command flags */
#define CMD_NEED_APP 0x80000000
#define CMD_RSPNS_48 0x00020000
#define CMD_ERRORS_MASK 0xfff9c004
#define CMD_RCA_MASK 0xffff0000

/* Commands (pre-built with flags) */
#define CMD_GO_IDLE 0x00000000
#define CMD_ALL_SEND_CID 0x02010000
#define CMD_SEND_REL_ADDR 0x03020000
#define CMD_CARD_SELECT 0x07030000
#define CMD_SEND_IF_COND 0x08020000
#define CMD_STOP_TRANS 0x0C030000
#define CMD_READ_SINGLE 0x11220010
#define CMD_READ_MULTI 0x12220032
#define CMD_SET_BLOCKCNT 0x17020000
#define CMD_APP_CMD 0x37000000
#define CMD_SET_BUS_WIDTH (0x06020000 | CMD_NEED_APP)
#define CMD_SEND_OP_COND (0x29020000 | CMD_NEED_APP)
#define CMD_SEND_SCR (0x33220010 | CMD_NEED_APP)

/* STATUS bits */
#define SR_READ_AVAILABLE 0x00000800
#define SR_DAT_INHIBIT 0x00000002
#define SR_CMD_INHIBIT 0x00000001
#define SR_APP_CMD 0x00000020

/* INTERRUPT bits */
#define INT_DATA_TIMEOUT 0x00100000
#define INT_CMD_TIMEOUT 0x00010000
#define INT_READ_RDY 0x00000020
#define INT_CMD_DONE 0x00000001
#define INT_DATA_DONE 0x00000002
#define INT_ERROR_MASK 0x017E8000

/* CONTROL1 bits */
#define C1_SRST_DATA 0x04000000
#define C1_SRST_CMD 0x02000000
#define C1_SRST_HC 0x01000000
#define C1_TOUNIT_DIS 0x000f0000
#define C1_TOUNIT_MAX 0x000e0000
#define C1_CLK_GENSEL 0x00000020
#define C1_CLK_EN 0x00000004
#define C1_CLK_STABLE 0x00000002
#define C1_CLK_INTLEN 0x00000001

/* SLOTISR_VER */
#define HOST_SPEC_NUM 0x00ff0000
#define HOST_SPEC_NUM_SHIFT 16
#define HOST_SPEC_V2 1

/* SCR flags */
#define SCR_SD_BUS_WIDTH_4 0x00000400
#define SCR_SUPP_SET_BLKCNT 0x02000000
#define SCR_SUPP_CCS 0x00000001

/* ACMD41 response */
#define ACMD41_VOLTAGE 0x00ff8000
#define ACMD41_CMD_COMPLETE 0x80000000
#define ACMD41_CMD_CCS 0x40000000
#define ACMD41_ARG_HC 0x51ff8000

/*
 * sdcard_init()
 *   Power up, clock the card, run CMD0→CMD8→ACMD41→CMD2→CMD3→CMD7→CMD16.
 *   Returns SD_OK on success.
 */
int sdcard_init(void);

/*
 * sdcard_read_block()
 *   Read one 512-byte sector from `lba` into `buf`.
 *   For SDHC/SDXC the argument is the LBA directly.
 *   For SDSC the argument is byte address (lba * 512).
 */
int sdcard_read_block(uint32_t lba, uint8_t *buf);

/*
 * sdcard_read_blocks()
 *   Read `count` consecutive sectors starting at `lba`.
 */
int sdcard_read_blocks(uint32_t lba, uint32_t count, uint8_t *buf);

#endif /* SDCARD_H */
