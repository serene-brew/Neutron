/* =============================================================================
 * Neutron Bootloader - Project Atom
 * boot/start.S  —  ARMv8 AArch64 bare-metal bootloader entry point
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * Toolchain : aarch64-none-elf-  (or aarch64-linux-gnu-)
 * Target    : QEMU -machine virt -cpu cortex-a53
 *
 * Binary layout (what lands at BOOTLOADER_LOAD_ADDR):
 *
 *   offset 0x000  : _start  (4 bytes: "b boot_entry")   ← PC starts HERE
 *   offset 0x004  : padding / literal pool
 *   offset 0x800  : vectors[]  (2 KiB, 2^11-aligned)
 *   offset 0x800+ : exc_* stubs, then boot_entry code
 *
 * _start MUST be at offset 0 so that when QEMU sets PC = LOAD_ADDR the
 * first instruction executed is ours, not a random byte of the vector table.
 *
 * QEMU -machine virt boots in EL2 (non-secure) with:
 *   x0 = DTB physical address,  x1–x3 = 0
 *
 * Bootloader will:
 *   1. Initialize CPU to EL1
 *   2. Set up memory (stack, BSS, data)
 *   3. Initialize UART for debug output
 *   4. Load kernel from storage/memory
 *   5. Jump to kernel at 0x40200000 with DTB in x0
 * ============================================================================= */

.section ".text.boot"
.global _start

/* ============================================================================
 * _start — must be the very first bytes of the binary
 * Jump over the vector table straight to boot_entry.
 * ============================================================================ */
_start:
    b       boot_entry          /* branch over the vector table                 */

/* ============================================================================
 * Literal pool for ldr x, =symbol instructions used in boot_entry.
 * Placed right after _start so they are within ±1 MiB of boot_entry.
 * ============================================================================ */
    .align 3
.Lstack_top:        .quad   _stack_top
.Lbss_start:        .quad   _bss_start
.Lbss_end:          .quad   _bss_end
.Ldata_load_start:  .quad   _data_load_start
.Ldata_start:       .quad   _data_start
.Ldata_end:         .quad   _data_end

/* ============================================================================
 * AArch64 Exception Vector Table
 *
 * VBAR_EL1 must point here — 2 KiB (2^11) aligned.
 * 16 slots × 128 bytes (2^7) each.
 * Groups: Current-EL/SP0, Current-EL/SPx, Lower-AArch64, Lower-AArch32
 * Types per group: Sync, IRQ, FIQ, SError
 * ============================================================================ */
.macro VENTRY label
    .align 7
    b       \label
.endm

    .align 11                           /* 2 KiB boundary for VBAR_EL1          */
vectors:
    VENTRY  exc_sync                    /* Current EL, SP_EL0                   */
    VENTRY  exc_irq
    VENTRY  exc_fiq
    VENTRY  exc_serror
    VENTRY  exc_sync                    /* Current EL, SP_ELx                   */
    VENTRY  exc_irq
    VENTRY  exc_fiq
    VENTRY  exc_serror
    VENTRY  exc_sync                    /* Lower EL, AArch64                    */
    VENTRY  exc_irq
    VENTRY  exc_fiq
    VENTRY  exc_serror
    VENTRY  exc_sync                    /* Lower EL, AArch32                    */
    VENTRY  exc_irq
    VENTRY  exc_fiq
    VENTRY  exc_serror

/* Stub exception handlers — spin so a debugger can catch them                 */
exc_sync:    b   exc_sync
exc_irq:     b   exc_irq
exc_fiq:     b   exc_fiq
exc_serror:  b   exc_serror

/* ============================================================================
 * boot_entry — main boot sequence, entered from _start via branch
 * ============================================================================ */
boot_entry:

    /* ---- 1. Which Exception Level are we in? ----------------------------- */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2                  /* EL in bits [1:0] of x0              */
    cmp     x0, #2
    bne     el1_setup                   /* skip EL2 config if already at EL1   */

/* ---- EL2 → EL1 drop ------------------------------------------------------- */
el2_setup:
    /* HCR_EL2: RW=1 (EL1 is AArch64), HCD=1 (disable HVC trap)             */
    mov     x0, #(1 << 31)
    orr     x0, x0, #(1 << 29)
    msr     hcr_el2, x0

    /* SCTLR_EL2: reset — MMU off, caches off                                */
    msr     sctlr_el2, xzr

    /* CPTR_EL2: clear TFP [10] so FP/SIMD works at EL1                     */
    mrs     x0, cptr_el2
    bic     x0, x0, #(1 << 10)
    msr     cptr_el2, x0

    /* CNTHCTL_EL2: let EL1 read the physical timer                          */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    /* SPSR_EL2: return to EL1h (0b00101), all interrupts masked (DAIF=1111)
     *   [4:0] = 0b00101  EL1h
     *   [9:6] = 0b1111   DAIF masked
     *   → 0x3C5                                                              */
    mov     x0, #0x3C5
    msr     spsr_el2, x0

    /* ELR_EL2: where eret should resume                                     */
    adr     x0, el1_setup
    msr     elr_el2, x0

    eret                                /* → EL1h, continues at el1_setup      */

/* ---- EL1 initialisation --------------------------------------------------- */
el1_setup:
    /* Use SP_EL1 (dedicated stack pointer for EL1)                          */
    msr     spsel, #1

    /* Disable MMU, D-cache, I-cache in SCTLR_EL1                           */
    mrs     x0, sctlr_el1
    mov     x1, #((1 << 0) | (1 << 2) | (1 << 12))    /* M | C | I          */
    bic     x0, x0, x1
    msr     sctlr_el1, x0
    isb

    /* Invalidate TLBs and I-cache                                           */
    tlbi    vmalle1
    ic      iallu
    dsb     sy
    isb

    /* Install vector table                                                   */
    adr     x0, vectors
    msr     vbar_el1, x0
    isb

    /* ---- 2. Stack -------------------------------------------------------- */
    adr     x0, .Lstack_top
    ldr     x0, [x0]
    mov     sp, x0

    /* ---- 3. Zero .bss ---------------------------------------------------- */
    adr     x4, .Lbss_start
    ldr     x4, [x4]
    adr     x5, .Lbss_end
    ldr     x5, [x5]
bss_loop:
    cmp     x4, x5
    bge     bss_done
    str     xzr, [x4], #8
    b       bss_loop
bss_done:

    /* ---- 4. Copy .data LMA → VMA ---------------------------------------- */
    adr     x4, .Ldata_load_start
    ldr     x4, [x4]
    adr     x5, .Ldata_start
    ldr     x5, [x5]
    adr     x6, .Ldata_end
    ldr     x6, [x6]
    cmp     x4, x5
    beq     data_done
data_loop:
    cmp     x5, x6
    bge     data_done
    ldr     x7, [x4], #8
    str     x7, [x5], #8
    b       data_loop
data_done:

    /* ---- 5. Save DTB address and enter bootloader ----------------------- */
    /* x0 still contains DTB address from QEMU firmware (passed at boot)      */
    mov     x10, x0                     /* x10 = DTB address (callee-saved)   */

    bl      bootloader_main
    b       .                           /* hang if bootloader_main returns    */
