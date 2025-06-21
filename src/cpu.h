#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CPU_FLAG_IF (1 << 9)

[[gnu::always_inline, gnu::artificial]]
inline uint32_t cpu_get_flags(void) {
    uint32_t eflags;
    __asm__ volatile("pushf\n"
                     "pop %0"
                     : "=g"(eflags));
    return eflags;
}

[[gnu::always_inline, gnu::artificial]]
inline bool cpu_check_interrupts(void) {
    return cpu_get_flags() & CPU_FLAG_IF;
}
