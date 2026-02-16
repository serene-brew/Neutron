/* =============================================================================
 * Neutron Bootloader - Project Atom
 * include/bootloader.h  —  Bootloader API and memory layout definitions
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * This header defines the contract between bootloader and kernel:
 * - Memory addresses
 * - Bootloader information passed to kernel
 * - Function prototypes
 * ============================================================================= */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stdint.h>

/* Memory Layout  ============================================================ */
#define BOOTLOADER_BASE     0x40000000UL    /* Bootloader load address       */
#define BOOTLOADER_SIZE     0x00200000UL    /* 2 MB max for bootloader       */
#define KERNEL_BASE         0x40200000UL    /* Kernel load address           */
#define DTB_BASE            0x40000000UL    /* DTB passed by firmware        */

/* Load address and size definitions ========================================= */
#define KERNEL_IMAGE_LOAD_ADDR  0x40400000UL  /* Where firmware loads kernel (staging area) */
#define MAX_KERNEL_SIZE         0x01000000UL  /* Max 16 MB for kernel */

/* Bootloader Information Passed to Kernel ================================== */
typedef struct {
    uint64_t dtb_phys_addr;                 /* Physical address of DTB       */
    uint64_t dtb_size;                      /* DTB size in bytes             */
    uint64_t bootloader_version;            /* Version for compatibility     */
    uint64_t bootloader_flags;              /* Feature flags                 */
} bootloader_info_t;

/* Bootloader Information — stored at known location for kernel access      */
extern bootloader_info_t bootloader_info;

/* Function Prototypes ======================================================= */

/* Main bootloader entry point (called from assembly)                        */
void bootloader_main(void);

/* Initialize bootloader info structure                                      */
void bootloader_info_init(uint64_t dtb_addr);

/* Load kernel from storage/memory to KERNEL_BASE                           */
int bootloader_load_kernel(void);

/* Jump to kernel at KERNEL_BASE with DTB address in x0                     */
void bootloader_jump_to_kernel(uint64_t dtb_addr) __attribute__((noreturn));

/* Minimal print function for bootloader debug output                        */
void bootloader_printf(const char *fmt, ...);

/* Sleep for milliseconds (busy loop) */
void bootloader_sleep(uint32_t milliseconds);
#endif /* BOOTLOADER_H */
