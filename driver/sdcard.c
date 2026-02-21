/* =============================================================================
 * Neutron Bootloader - Project Atom
 * driver/sdcard.c  -  BCM2710 Arasan SDHCI SD Card Driver
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Controller: Arasan SDHCI at MMIO_BASE + 0x300000
 * ================================================================ */

#include "sdcard.h"
#include "gpio.h"
#include "platform.h"
#include "uart.h"
#include <stddef.h>
#include <stdint.h>

/* State */
static uint32_t sd_scr[2];
static uint32_t sd_rca = 0;
static uint32_t sd_err = 0;
static uint32_t sd_hv = 0;

/* Simple cycle delay */
static void wait_cycles(uint32_t n) {
  if (n)
    while (n--) {
      __asm__ volatile("nop");
    }
}

/* Wait N milliseconds using ARM CPU physical counter - accurate timing */
static void wait_msec(uint32_t n) {
  register unsigned long f, t, r;
  /* Get the current counter frequency */
  __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
  /* Read the current counter */
  __asm__ volatile("mrs %0, cntpct_el0" : "=r"(t));
  /* Calculate required count increase (f/1000 = ticks per ms) */
  unsigned long i = (f / 1000) * n;
  /* Loop while counter increase is less than i */
  do {
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(r));
  } while ((r - t) < i);
}

/* Wait for status bits - with delays between polls */
static int sd_status(uint32_t mask) {
  int cnt = 500000;
  while ((*EMMC_STATUS & mask) && !(*EMMC_INTERRUPT & INT_ERROR_MASK) &&
         cnt--) {
    wait_msec(1);
  }
  if (cnt <= 0 || (*EMMC_INTERRUPT & INT_ERROR_MASK)) {
    return SD_ERR_TIMEOUT;
  }
  return SD_OK;
}

/* Wait for interrupt - with delays between polls (critical for SDHCI) */
static int sd_int(uint32_t mask) {
  uint32_t r, m = mask | INT_ERROR_MASK;
  int cnt = 1000000;

  while (!(*EMMC_INTERRUPT & m) && cnt--) {
    wait_msec(1);
  }

  r = *EMMC_INTERRUPT;
  if (cnt <= 0 || (r & INT_CMD_TIMEOUT) || (r & INT_DATA_TIMEOUT)) {
    *EMMC_INTERRUPT = r;
    return SD_ERR_TIMEOUT;
  }
  if (r & INT_ERROR_MASK) {
    *EMMC_INTERRUPT = r;
    return SD_ERR_CMD;
  }
  *EMMC_INTERRUPT = mask;
  return SD_OK;
}

/* Send a command */
static int sd_cmd(uint32_t code, uint32_t arg) {
  int r = 0;
  sd_err = SD_OK;

  if (code & CMD_NEED_APP) {
    r = sd_cmd(CMD_APP_CMD | (sd_rca ? CMD_RSPNS_48 : 0), sd_rca);
    if (sd_rca && !r) {
      uart_puts("[SD]  ERROR: failed to send SD APP command\n");
      sd_err = SD_ERR_CMD;
      return 0;
    }
    code &= ~CMD_NEED_APP;
  }

  if (sd_status(SR_CMD_INHIBIT)) {
    uart_puts("[SD]  ERROR: EMMC busy\n");
    sd_err = SD_ERR_TIMEOUT;
    return 0;
  }

  *EMMC_INTERRUPT = *EMMC_INTERRUPT;
  *EMMC_ARG1 = arg;
  *EMMC_CMDTM = code;

  if (code == CMD_SEND_OP_COND) {
    wait_msec(1000);
  } else if (code == CMD_SEND_IF_COND || code == CMD_APP_CMD) {
    wait_msec(100);
  }

  if ((r = sd_int(INT_CMD_DONE))) {
    uart_puts("[SD]  ERROR: failed to send EMMC command\n");
    sd_err = r;
    return 0;
  }

  r = *EMMC_RESP0;

  if (code == CMD_GO_IDLE || code == CMD_APP_CMD) {
    return 0;
  } else if (code == (CMD_APP_CMD | CMD_RSPNS_48)) {
    return r & SR_APP_CMD;
  } else if (code == CMD_SEND_OP_COND) {
    return r;
  } else if (code == CMD_SEND_IF_COND) {
    return ((uint32_t)r == arg) ? SD_OK : SD_ERR_VOLTAGE;
  } else if (code == CMD_ALL_SEND_CID) {
    r |= *EMMC_RESP3;
    r |= *EMMC_RESP2;
    r |= *EMMC_RESP1;
    return r;
  } else if (code == CMD_SEND_REL_ADDR) {
    sd_err = (((r & 0x1fff)) | ((r & 0x2000) << 6) | ((r & 0x4000) << 8) |
              ((r & 0x8000) << 8)) &
             CMD_ERRORS_MASK;
    return r & CMD_RCA_MASK;
  }

  return r & CMD_ERRORS_MASK;
}

/* Set SD clock frequency in Hz */
static int sd_clk(uint32_t f) {
  uint32_t d, c = 41666666 / f, x, s = 32, h = 0;
  int cnt = 100000;

  while ((*EMMC_STATUS & (SR_CMD_INHIBIT | SR_DAT_INHIBIT)) && cnt--) {
    wait_msec(1);
  }
  if (cnt <= 0) {
    uart_puts("[SD]  ERROR: timeout waiting for inhibit flag\n");
    return SD_ERR_TIMEOUT;
  }

  *EMMC_CONTROL1 &= ~C1_CLK_EN;
  wait_msec(10);

  x = c - 1;
  if (!x) {
    s = 0;
  } else {
    if (!(x & 0xffff0000u)) {
      x <<= 16;
      s -= 16;
    }
    if (!(x & 0xff000000u)) {
      x <<= 8;
      s -= 8;
    }
    if (!(x & 0xf0000000u)) {
      x <<= 4;
      s -= 4;
    }
    if (!(x & 0xc0000000u)) {
      x <<= 2;
      s -= 2;
    }
    if (!(x & 0x80000000u)) {
      x <<= 1;
      s -= 1;
    }
    if (s > 0)
      s--;
    if (s > 7)
      s = 7;
  }

  if (sd_hv > HOST_SPEC_V2) {
    d = c;
  } else {
    d = (1 << s);
  }
  if (d <= 2) {
    d = 2;
    s = 0;
  }

  if (sd_hv > HOST_SPEC_V2) {
    h = (d & 0x300) >> 2;
  }
  d = ((d & 0x0ff) << 8) | h;

  *EMMC_CONTROL1 = (*EMMC_CONTROL1 & 0xffff003f) | d;
  wait_msec(10);
  *EMMC_CONTROL1 |= C1_CLK_EN;
  wait_msec(10);

  cnt = 10000;
  while (!(*EMMC_CONTROL1 & C1_CLK_STABLE) && cnt--) {
    wait_msec(10);
  }
  if (cnt <= 0) {
    uart_puts("[SD]  ERROR: failed to get stable clock\n");
    return SD_ERR_TIMEOUT;
  }

  return SD_OK;
}

/* Initialize EMMC */
int sdcard_init(void) {
  uint32_t r, cnt, ccs = 0;

  uart_puts("[SD]  Initialising Arasan SDHCI...\n");

  /* GPIO setup for SD card pins - CRITICAL for operation */
  /* GPIO 47: CD (Card Detect) - input with pull-up */
  gpio_set_func(47, GPIO_FUNC_INPUT);
  gpio_set_pull(47, GPIO_PULL_UP);

  /* GPIO 48: CLK - ALT3 (EMMC) */
  gpio_set_func(48, GPIO_FUNC_ALT3);
  gpio_set_pull(48, GPIO_PULL_UP);

  /* GPIO 49: CMD - ALT3 (EMMC) */
  gpio_set_func(49, GPIO_FUNC_ALT3);
  gpio_set_pull(49, GPIO_PULL_UP);

  /* GPIO 50-53: DAT0-3 - ALT3 (EMMC) */
  for (int pin = 50; pin <= 53; pin++) {
    gpio_set_func(pin, GPIO_FUNC_ALT3);
    gpio_set_pull(pin, GPIO_PULL_UP);
  }

  uart_puts("[SD]  GPIO configured\n");

  sd_hv = (*EMMC_SLOTISR_VER & HOST_SPEC_NUM) >> HOST_SPEC_NUM_SHIFT;
  uart_printf("[SD]  SDHCI version: %d\n", sd_hv);

  /* Reset the controller */
  *EMMC_CONTROL0 = 0;
  *EMMC_CONTROL1 |= C1_SRST_HC;
  cnt = 10000;
  do {
    wait_msec(10);
  } while ((*EMMC_CONTROL1 & C1_SRST_HC) && cnt--);

  if (cnt <= 0) {
    uart_puts("[SD]  ERROR: failed to reset EMMC\n");
    return SD_ERR_RESET;
  }
  uart_puts("[SD]  Reset OK\n");

  *EMMC_CONTROL1 |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
  wait_msec(10);

  /* Set clock to 400kHz for init */
  if ((r = sd_clk(400000))) {
    return r;
  }

  *EMMC_INT_EN = 0xffffffff;
  *EMMC_INT_MASK = 0xffffffff;

  sd_scr[0] = sd_scr[1] = sd_rca = sd_err = 0;

  /* CMD0 - go idle */
  sd_cmd(CMD_GO_IDLE, 0);
  if (sd_err) {
    return sd_err;
  }

  /* CMD8 - check voltage */
  sd_cmd(CMD_SEND_IF_COND, 0x000001AA);
  if (sd_err) {
    uart_puts("[SD]  CMD8 failed\n");
    return sd_err;
  }
  uart_puts("[SD]  CMD8 OK\n");

  /* ACMD41 - wait for card ready */
  cnt = 6;
  r = 0;
  while (!(r & ACMD41_CMD_COMPLETE) && cnt--) {
    wait_cycles(400);
    r = sd_cmd(CMD_SEND_OP_COND, ACMD41_ARG_HC);
    if (sd_err != SD_ERR_TIMEOUT && sd_err != SD_OK) {
      uart_puts("[SD]  ERROR: EMMC ACMD41 returned error\n");
      return sd_err;
    }
  }

  if (!(r & ACMD41_CMD_COMPLETE) || !cnt) {
    uart_puts("[SD]  ERROR: ACMD41 timeout\n");
    return SD_ERR_TIMEOUT;
  }
  if (!(r & ACMD41_VOLTAGE)) {
    uart_puts("[SD]  ERROR: ACMD41 voltage\n");
    return SD_ERR_VOLTAGE;
  }
  if (r & ACMD41_CMD_CCS) {
    ccs = SCR_SUPP_CCS;
  }

  uart_puts("[SD]  Card ready\n");

  /* CMD2 - get CID */
  sd_cmd(CMD_ALL_SEND_CID, 0);

  /* CMD3 - get RCA */
  sd_rca = sd_cmd(CMD_SEND_REL_ADDR, 0);
  if (sd_err) {
    return sd_err;
  }
  uart_printf("[SD]  RCA: %08X\n", sd_rca);

  /* Set clock to 25MHz */
  if ((r = sd_clk(25000000))) {
    return r;
  }

  /* CMD7 - select card */
  sd_cmd(CMD_CARD_SELECT, sd_rca);
  if (sd_err) {
    return sd_err;
  }

  /* Read SCR to determine CCS (card capacity status) */
  if (sd_status(SR_DAT_INHIBIT)) {
    return SD_ERR_TIMEOUT;
  }

  *EMMC_BLKSIZECNT = (1 << 16) | 8;
  sd_cmd(CMD_SEND_SCR, 0);
  if (sd_err) {
    return sd_err;
  }

  if (sd_int(INT_READ_RDY)) {
    return SD_ERR_TIMEOUT;
  }

  r = 0;
  cnt = 100000;
  while (r < 2 && cnt) {
    if (*EMMC_STATUS & SR_READ_AVAILABLE) {
      sd_scr[r++] = *EMMC_DATA;
    } else {
      wait_msec(1);
    }
  }
  if (r != 2) {
    return SD_ERR_TIMEOUT;
  }

  /* Set 4-bit bus width if supported */
  if (sd_scr[0] & SCR_SD_BUS_WIDTH_4) {
    sd_cmd(CMD_SET_BUS_WIDTH, sd_rca | 2);
    if (sd_err) {
      return sd_err;
    }
    *EMMC_CONTROL0 |= 0x00000002; /* C0_HCTL_DWITDH */
  }

  /* Add CCS software flag based on ACMD41 response */
  sd_scr[0] &= ~SCR_SUPP_CCS;
  sd_scr[0] |= ccs;

  if (ccs) {
    uart_puts("[SD]  Type: SDHC/SDXC (block addressing)\n");
  } else {
    uart_puts("[SD]  Type: SDSC (byte addressing)\n");
  }

  uart_puts("[SD]  SD card initialised successfully.\n");
  return SD_OK;
}

/* Read block(s) from SD card */
int sdcard_read_block(uint32_t lba, uint8_t *buf) {
  return sdcard_read_blocks(lba, 1, buf);
}

int sdcard_read_blocks(uint32_t lba, uint32_t num, uint8_t *buf) {
  int r, d;
  uint32_t c = 0;

  // uart_puts("[SD]  read_blocks START lba="); uart_puthex32(lba); uart_puts("
  // num="); uart_puthex32(num); uart_puts("\n");

  if (num < 1)
    num = 1;
  if (sd_status(SR_DAT_INHIBIT)) {
    sd_err = SD_ERR_TIMEOUT;
    uart_puts("[SD]  ERROR: DAT_INHIBIT timeout\n");
    return SD_ERR_TIMEOUT;
  }
  // uart_puts("[SD]  DAT_INHIBIT OK\n");

  uint32_t *dest = (uint32_t *)buf;

  /* SDHC/SDXC use block addressing, SDSC uses byte addressing */
  if (sd_scr[0] & SCR_SUPP_CCS) {
    // uart_puts("[SD]  CCS mode - block addressing\n");
    /* Block addressing for SDHC/SDXC */
    if (num > 1 && (sd_scr[0] & SCR_SUPP_SET_BLKCNT)) {
      sd_cmd(CMD_SET_BLOCKCNT, num);
      if (sd_err)
        return SD_ERR_CMD;
    }
    *EMMC_BLKSIZECNT = (num << 16) | 512;
    sd_cmd(num == 1 ? CMD_READ_SINGLE : CMD_READ_MULTI, lba);
    if (sd_err)
      return SD_ERR_CMD;
  } else {
    // uart_puts("[SD]  SDSC mode - byte addressing\n");
    /* Byte addressing for SDSC */
    *EMMC_BLKSIZECNT = (1 << 16) | 512;
  }

  while (c < num) {
    // uart_puts("[SD]  block loop c="); uart_puthex32(c); uart_puts("\n");
    if (!(sd_scr[0] & SCR_SUPP_CCS)) {
      /* SDSC: need to send command for each block */
      // uart_puts("[SD]  sending CMD17...\n");
      sd_cmd(CMD_READ_SINGLE, (lba + c) * 512);
      if (sd_err) {
        uart_puts("[SD]  ERROR: CMD17 failed\n");
        return SD_ERR_CMD;
      }
      // uart_puts("[SD]  CMD17 sent OK\n");
    }
    if ((r = sd_int(INT_READ_RDY))) {
      uart_puts("[SD]  ERROR: Timeout waiting for ready to read\n");
      sd_err = r;
      return SD_ERR_TIMEOUT;
    }
    for (d = 0; d < 128; d++) {
      dest[d] = *EMMC_DATA;
    }
    c++;
    dest += 128;
  }

  /* Stop transmission for multi-block reads if needed */
  if (num > 1 && !(sd_scr[0] & SCR_SUPP_SET_BLKCNT) &&
      (sd_scr[0] & SCR_SUPP_CCS)) {
    sd_cmd(CMD_STOP_TRANS, 0);
  }

  // uart_puts("[SD]  read_blocks DONE\n");
  return (sd_err != SD_OK || c != num) ? SD_ERR_DATA : SD_OK;
}
