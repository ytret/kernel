#pragma once

#include <stdint.h>

void gdt_init(void);
void gdt_load(uint8_t const * p_desc);
