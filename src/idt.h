#pragma once

#include <stdint.h>

typedef struct {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed)) int_frame_t;

void idt_init(void);
void idt_dummy_exception_handler(uint32_t exc_num, uint32_t err_code,
                                 int_frame_t *p_stack_frame);
void idt_dummy_handler(int_frame_t *p_stack_frame);

// Defined in idt.s.
void idt_load(uint8_t const *p_desc);
