#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t * p_bottom;
    uint32_t * p_top;
    uint32_t * p_top_max;
} stack_t;

void stack_new(stack_t * p_stack, void * p_bottom, size_t size_bytes);

void     stack_push(stack_t * p_stack, uint32_t value);
uint32_t stack_pop(stack_t * p_stack);

bool   stack_is_full(stack_t const * p_stack);
bool   stack_is_empty(stack_t const * p_stack);
