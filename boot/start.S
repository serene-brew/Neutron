/* =============================================================================
 * Neutron Bootloader - Project Atom
 * boot/start.S  -  Neutron Bootloader Entry Point
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
* Target : ARMv8-A (AArch64)  BCM2710 / Raspberry Pi Zero 2W
 * Loaded : GPU places kernel8.img at 0x80000
 * ============================================================ */

#include "aarch64.h"

.section ".text.boot"
.global _start

/* ----------------------------------------------------------------
 * _start  -  first instruction executed after GPU hands off
 * All four cores start here; park cores 1-3 immediately.
 * ---------------------------------------------------------------- */
_start:
    /* ---- 1. Park secondary cores ---- */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF          // core ID in bits [7:0]
    cbnz    x0, _park_core         // non-zero → secondary, spin forever

    /* ---- 2. Determine current Exception Level ---- */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2             // CurrentEL[3:2] holds EL
    cmp     x0, #2
    beq     _from_el2              // most QEMU/HW boot at EL2
    cmp     x0, #1
    beq     _el1_entry             // already EL1, unlikely but handle
    b       _park_core             // EL0 / EL3 → not expected, park

/* ----------------------------------------------------------------
 * _from_el2  -  configure HCR_EL2 and drop to EL1
 * ---------------------------------------------------------------- */
_from_el2:
    /* Allow AArch64 at EL1 */
    mov     x0, #(1 << 31)         // HCR_EL2.RW = 1  (EL1 is AArch64)
    msr     hcr_el2, x0

    /* Minimal SCTLR_EL1 - disable MMU, caches, alignment checks */
    mov     x0, xzr
    msr     sctlr_el1, x0
    isb

    /* SPSR_EL2: mask all interrupts, AArch64, EL1h (use SP_EL1) */
    mov     x0, #(SPSR_MASK_ALL | SPSR_EL1H)
    msr     spsr_el2, x0

    /* ELR_EL2: return address = _el1_entry */
    adr     x0, _el1_entry
    msr     elr_el2, x0

    eret                            // drop to EL1

/* ----------------------------------------------------------------
 * _el1_entry  -  running at EL1, finish CPU init
 * ---------------------------------------------------------------- */
_el1_entry:
    /* ---- Disable MMU / caches at EL1 (safe defaults) ---- */
    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 0)     // M   = 0  disable MMU
    bic     x0, x0, #(1 << 2)     // C   = 0  disable D-cache
    bic     x0, x0, #(1 << 12)    // I   = 0  disable I-cache
    msr     sctlr_el1, x0
    isb

    /* ---- Set up stack (grows downward from _start = 0x80000) ---- */
    ldr     x0, =_start
    mov     sp, x0

    /* ---- Zero BSS ---- */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    cmp     x0, x1
    bge     _bss_done
_bss_loop:
    str     xzr, [x0], #8
    cmp     x0, x1
    blt     _bss_loop
_bss_done:

    /* ---- Jump to C bootloader entry ---- */
    bl      neutron_main

    /* If main ever returns, fall through to park */
_park_core:
    wfe
    b       _park_core

/* ----------------------------------------------------------------
 * Exception Vector Table (minimal - just in case EL1 takes a fault)
 * ---------------------------------------------------------------- */
.balign 0x800
.global exception_vector_table
exception_vector_table:
    /* Each slot is 0x80 bytes (32 instructions) */
    /* --- Current EL, SP_EL0 --- */
    b       _unhandled_exception    // Sync
    .balign 0x80
    b       _unhandled_exception    // IRQ
    .balign 0x80
    b       _unhandled_exception    // FIQ
    .balign 0x80
    b       _unhandled_exception    // SError
    /* --- Current EL, SP_ELx --- */
    .balign 0x80
    b       _unhandled_exception
    .balign 0x80
    b       _unhandled_exception
    .balign 0x80
    b       _unhandled_exception
    .balign 0x80
    b       _unhandled_exception

_unhandled_exception:
    /* Spin - a real bootloader would print ESR/ELR and halt */
    wfe
    b       _unhandled_exception
