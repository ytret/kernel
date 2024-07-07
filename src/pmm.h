#pragma once

#include <stdint.h>

void pmm_init(void);

void pmm_push_page(uint32_t addr);
uint32_t pmm_pop_page(void);
