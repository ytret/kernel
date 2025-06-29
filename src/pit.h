#pragma once

#include <stdint.h>

#define PIT_IRQ       0
#define PIT_PERIOD_MS 10

void pit_init(uint8_t period_ms);
uint64_t pit_counter_ms(void);
void pit_irq_handler(void);
