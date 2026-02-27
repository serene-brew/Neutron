/* ================================================================
 * Neutron Bootloader - Project Atom
 * include/aarch64.h  -  AArch64 register / constant definitions
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 * ================================================================ */

#ifndef AARCH64_H
#define AARCH64_H

/* SPSR_EL2 bits for dropping to EL1 */
#define SPSR_MODE_EL1H 0x05 /* EL1h  (use SP_EL1)          */
#define SPSR_F_BIT (1 << 6) /* FIQ masked                  */
#define SPSR_I_BIT (1 << 7) /* IRQ masked                  */
#define SPSR_A_BIT (1 << 8) /* SError masked               */
#define SPSR_D_BIT (1 << 9) /* Debug masked                */
#define SPSR_MASK_ALL (SPSR_D_BIT | SPSR_A_BIT | SPSR_I_BIT | SPSR_F_BIT)
#define SPSR_EL1H SPSR_MODE_EL1H

/* HCR_EL2 */
#define HCR_RW (1 << 31) /* EL1 is AArch64              */

/* SCTLR_EL1 */
#define SCTLR_MMU_EN (1 << 0)
#define SCTLR_A_EN (1 << 1)
#define SCTLR_DCACHE_EN (1 << 2)
#define SCTLR_ICACHE_EN (1 << 12)

#endif /* AARCH64_H */
