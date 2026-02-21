/* =============================================================================
 * Neutron Bootloader - Project Atom
 * include/gpio.h  —  BCM2710 GPIO driver interface
 *
 * Organization : serene brew
 * Author       : mintRaven-05
 * License      : BSD-3-Clause
 *
 *   - QEMU raspi3b GPIO driver at 0x3F200000 (MMIO)
 * =============================================================================
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

typedef enum {
  GPIO_FUNC_INPUT = 0,
  GPIO_FUNC_OUTPUT = 1,
  GPIO_FUNC_ALT0 = 4,
  GPIO_FUNC_ALT1 = 5,
  GPIO_FUNC_ALT2 = 6,
  GPIO_FUNC_ALT3 = 7,
  GPIO_FUNC_ALT4 = 3,
  GPIO_FUNC_ALT5 = 2,
} gpio_func_t;

typedef enum {
  GPIO_PULL_NONE = 0,
  GPIO_PULL_DOWN = 1,
  GPIO_PULL_UP = 2,
} gpio_pull_t;

void gpio_set_func(uint32_t pin, gpio_func_t func);
void gpio_set_pull(uint32_t pin, gpio_pull_t pull);
void gpio_set(uint32_t pin);
void gpio_clear(uint32_t pin);
uint32_t gpio_get(uint32_t pin);

#endif /* GPIO_H */
