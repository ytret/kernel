#pragma once

#include <stdint.h>

void idt_init(void);

// Defined in idt.s.
//
void idt_load(uint8_t const * p_desc);
