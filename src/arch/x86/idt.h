#pragma once

#include <stdint.h>

#include "isrs.h"

void idt_init(void);
uint8_t *idt_get_desc(void);
void idt_dummy_exception_handler(uint32_t exc_num, uint32_t err_code,
                                 isr_stack_frame_t *p_stack_frame);
void idt_dummy_handler(isr_stack_frame_t *p_stack_frame);
void idt_page_fault_handler(uint32_t addr, uint32_t err_code,
                            isr_stack_frame_t *p_stack_frame);

// Defined in idt.s.
void idt_load(uint8_t const *p_desc);
