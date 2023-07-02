#pragma once

#include <mbi.h>

#include <stdint.h>

void pmm_init(mbi_t const * p_mbi);

void     pmm_push_page(uint32_t addr);
uint32_t pmm_pop_page(void);
