/* ================================================================
 * Neutron Bootloader - Project Atom
 * include/mbox.h  -  BCM2710 Mailbox / Property Tag interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 * ================================================================ */

#ifndef MBOX_H
#define MBOX_H

/* ================================================================
 * ================================================================ */

#include <stdint.h>

/* Return codes */
#define MBOX_SUCCESS 0
#define MBOX_ERROR 1

/* Maximum property buffer size (in 32-bit words) */
#define MBOX_BUF_SIZE 36

/*
 * mbox_call()
 *   Send a property tag buffer on the given channel.
 *   buf must be 16-byte aligned.
 *   Returns MBOX_SUCCESS / MBOX_ERROR.
 */
int mbox_call(volatile uint32_t *buf, uint8_t channel);

/* Convenience wrappers */
uint32_t mbox_get_board_revision(void);
uint32_t mbox_get_arm_mem_size(void);

#endif /* MBOX_H */
