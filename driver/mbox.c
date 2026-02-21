/* ================================================================
 * Neutron Bootloader - Project Atom
 * driver/mbox.c  -  BCM2710 VideoCore Mailbox / Property Tags
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 *
 * The mailbox buffer must be 16-byte aligned.
 * We allocate it as a static aligned array.
 * ================================================================ */

#include "mbox.h"
#include "platform.h"
#include <stdint.h>

/* ----------------------------------------------------------------
 * MMIO helpers
 * ---------------------------------------------------------------- */
static inline void mmio_write(uintptr_t addr, uint32_t val) {
  *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read(uintptr_t addr) {
  return *(volatile uint32_t *)addr;
}

/* ----------------------------------------------------------------
 * mbox_call()
 *   buf[0] = total buffer size in bytes
 *   buf[1] = request/response code
 *   buf[2..n-2] = tag data
 *   buf[n-1] = 0 (end tag)
 * ---------------------------------------------------------------- */
int mbox_call(volatile uint32_t *buf, uint8_t channel) {
  /* Lower 4 bits are channel; upper 28 bits are buffer address */
  uint32_t msg = ((uint32_t)(uintptr_t)buf & ~0xFU) | (channel & 0xFU);

  /* Wait until mailbox is not full */
  while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_FULL)
    ;

  /* Write the message */
  mmio_write(MBOX_BASE + MBOX_WRITE, msg);

  /* Poll for a response on our channel */
  while (1) {
    while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_EMPTY)
      ;

    uint32_t resp = mmio_read(MBOX_BASE + MBOX_READ);
    if ((resp & 0xFU) == channel) {
      /* buf[1] should be 0x80000000 on success */
      return (buf[1] == 0x80000000U) ? MBOX_SUCCESS : MBOX_ERROR;
    }
  }
}

/* ----------------------------------------------------------------
 * mbox_get_board_revision()
 * ---------------------------------------------------------------- */
static volatile uint32_t __attribute__((aligned(16))) mbox_buf[MBOX_BUF_SIZE];

uint32_t mbox_get_board_revision(void) {
  mbox_buf[0] = 7 * 4; /* buffer size         */
  mbox_buf[1] = 0;     /* request             */
  mbox_buf[2] = MBOX_TAG_GET_BOARD_REVISION;
  mbox_buf[3] = 4; /* value buffer size   */
  mbox_buf[4] = 0; /* request/response    */
  mbox_buf[5] = 0; /* value (output)      */
  mbox_buf[6] = MBOX_TAG_LAST;

  if (mbox_call(mbox_buf, MBOX_CH_PROP) == MBOX_SUCCESS) {
    return mbox_buf[5];
  }
  return 0;
}

/* ----------------------------------------------------------------
 * mbox_get_arm_mem_size()
 * ---------------------------------------------------------------- */
uint32_t mbox_get_arm_mem_size(void) {
  mbox_buf[0] = 8 * 4; /* buffer size         */
  mbox_buf[1] = 0;     /* request             */
  mbox_buf[2] = MBOX_TAG_GET_ARM_MEM;
  mbox_buf[3] = 8; /* value buffer size   */
  mbox_buf[4] = 0; /* request/response    */
  mbox_buf[5] = 0; /* base address        */
  mbox_buf[6] = 0; /* size (output)       */
  mbox_buf[7] = MBOX_TAG_LAST;

  if (mbox_call(mbox_buf, MBOX_CH_PROP) == MBOX_SUCCESS) {
    return mbox_buf[6]; /* ARM memory size     */
  }
  return 0;
}
