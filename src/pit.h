#pragma once

#include <stdint.h>

#define PIT_IRQ         0
#define PIT_PERIOD_MS   10

void pit_init(uint8_t period_ms);
void pit_irq_handler(void);
