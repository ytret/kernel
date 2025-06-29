#include "cpu.h"

#define CPU_FLAG_IF    (1 << 9)

uint32_t cpu_get_flags(void) {
    uint32_t eflags;
    __asm__ volatile("pushf\n"
                     "pop %0"
                     : "=g"(eflags));
    return eflags;
}

bool cpu_get_int_flag(void) {
    return cpu_get_flags() & CPU_FLAG_IF;
}
