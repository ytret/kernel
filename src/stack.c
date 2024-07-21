#include "kprintf.h"
#include "panic.h"
#include "stack.h"

static void check_stack(stack_t const *p_stack);

void stack_new(stack_t *p_stack, void *p_bottom, size_t size_bytes) {
    if (0 == size_bytes) {
        kprintf("stack_new: refuse to create an empty stack\n");
        panic("invalid argument");
    }

    p_stack->p_bottom = ((uint32_t *)p_bottom);
    p_stack->p_top = ((uint32_t *)(((uint32_t)p_bottom) + size_bytes));
    p_stack->p_top_max = p_stack->p_top;
}

void stack_push(stack_t *p_stack, uint32_t value) {
    if (stack_is_full(p_stack)) {
        // Cannot push the page - the stack is full.
        kprintf("stack_push: stack is full\n");
        panic("unexpected behavior");
    }

    p_stack->p_top--;
    *p_stack->p_top = value;
}

uint32_t stack_pop(stack_t *p_stack) {
    if (stack_is_empty(p_stack)) {
        // Cannot pop the page - the stack is empty.
        kprintf("stack_pop: stack is empty\n");
        panic("unexpected behavior");
    }

    return *p_stack->p_top++;
}

bool stack_is_full(stack_t const *p_stack) {
    check_stack(p_stack);
    return p_stack->p_top <= p_stack->p_bottom;
}

bool stack_is_empty(stack_t const *p_stack) {
    check_stack(p_stack);
    return p_stack->p_top >= p_stack->p_top_max;
}

static void check_stack(stack_t const *p_stack) {
    if (!p_stack) {
        kprintf("check_stack: p_stack is NULL\n");
        panic("unexpected behavior");
    }

    if (p_stack->p_top_max < p_stack->p_bottom) {
        kprintf("check_stack: top is below bottom\n");
        panic("invalid stack");
    }
}
