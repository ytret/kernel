#pragma once

#include <stdint.h>

#define PIT_IRQ       0
#define PIT_PERIOD_MS 10

void pit_init(uint8_t period_ms);
uint64_t pit_counter_ms(void);

/**
 * Waits for @a delay_ms milliseconds in a loop.
 * @param delay_ms Number of milliseconds to wait.
 * @warning
 * PIT interrupt must be enabled, and #pit_irq_handler() must be called in the
 * interrupt handler, otherwise this function does not return.
 */
void pit_delay_ms(uint32_t delay_ms);

void pit_irq_handler(void);
