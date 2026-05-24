#pragma once

#include <stdint.h>

void arch_timer_init(void);
uint64_t arch_timer_current_ms(void);
void arch_timer_busy_wait_ms(uint64_t msec);
