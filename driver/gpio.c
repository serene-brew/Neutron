/* =============================================================================
 * Neutron Bootloader - Project Atom
 * driver/gpio.c  -  BCM2710 GPIO driver
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 * ================================================================ */
#include "gpio.h"
#include "platform.h"
#include <stdint.h>

/* ----------------------------------------------------------------
 * MMIO helpers (volatile to prevent compiler optimisation)
 * ---------------------------------------------------------------- */
static inline void mmio_write(uintptr_t addr, uint32_t val) {
  *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read(uintptr_t addr) {
  return *(volatile uint32_t *)addr;
}

/* Simple busy-loop delay (rough cycles) */
static void gpio_delay(uint32_t cycles) {
  while (cycles--) {
    __asm__ volatile("nop");
  }
}

/* ----------------------------------------------------------------
 * gpio_set_func()
 *   Set the alternate function (or input/output) for `pin`.
 *   Each GPFSEL register controls 10 pins, 3 bits per pin.
 * ---------------------------------------------------------------- */
void gpio_set_func(uint32_t pin, gpio_func_t func) {
  uint32_t reg_offset = (pin / 10) * 4; /* GPFSEL0, 1, 2 ... */
  uint32_t bit_shift = (pin % 10) * 3;

  uintptr_t reg = GPIO_BASE + reg_offset;
  uint32_t val = mmio_read(reg);

  val &= ~(0x7U << bit_shift);          /* clear 3 bits        */
  val |= ((uint32_t)func << bit_shift); /* set new function    */

  mmio_write(reg, val);
}

/* ----------------------------------------------------------------
 * gpio_set_pull()
 *   Configure pull-up / pull-down resistor for `pin`.
 *   BCM2835/7 requires a timed sequence via GPPUD + GPPUDCLK.
 *   Supports pins 0-53 (GPPUDCLK0 for 0-31, GPPUDCLK1 for 32-53).
 * ---------------------------------------------------------------- */
void gpio_set_pull(uint32_t pin, gpio_pull_t pull) {
  uintptr_t gppud = GPIO_BASE + GPPUD; /* 0x94 */
  uintptr_t gppudclk =
      GPIO_BASE + (pin < 32 ? 0x98U : 0x9CU); /* GPPUDCLK0 or GPPUDCLK1 */

  mmio_write(gppud, (uint32_t)pull);
  gpio_delay(150);

  mmio_write(gppudclk, 1U << (pin & 31));
  gpio_delay(150);

  mmio_write(gppud, 0);
  mmio_write(gppudclk, 0);
}

/* ----------------------------------------------------------------
 * gpio_set() / gpio_clear() / gpio_get()
 * ---------------------------------------------------------------- */
void gpio_set(uint32_t pin) {
  /* GPSET0 = GPIO_BASE + 0x1C  (pins 0-31)
   * GPSET1 = GPIO_BASE + 0x20  (pins 32-53) */
  uintptr_t reg = GPIO_BASE + (pin < 32 ? 0x1CU : 0x20U);
  mmio_write(reg, 1U << (pin & 31));
}

void gpio_clear(uint32_t pin) {
  /* GPCLR0 = GPIO_BASE + 0x28  (pins 0-31)
   * GPCLR1 = GPIO_BASE + 0x2C  (pins 32-53) */
  uintptr_t reg = GPIO_BASE + (pin < 32 ? 0x28U : 0x2CU);
  mmio_write(reg, 1U << (pin & 31));
}

uint32_t gpio_get(uint32_t pin) {
  /* GPLEV0 = GPIO_BASE + 0x34  (pins 0-31) */
  uintptr_t reg = GPIO_BASE + (pin < 32 ? 0x34U : 0x38U);
  return (mmio_read(reg) >> (pin & 31)) & 1U;
}
