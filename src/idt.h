#pragma once

#include <stdint.h>

void idt_init(void);
void idt_load(uint8_t const * p_desc);
