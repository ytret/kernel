#include "cpu.h"

#define CPU_FLAG_IF (1 << 9)

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

void cpu_write_msr(uint32_t msr, uint64_t val) {
    const uint32_t hi = val >> 32;
    const uint32_t lo = val & 0xFFFFFFFF;
    __asm__ volatile("wrmsr"
                     : /* no output */
                     : "a"(lo), "d"(hi), "c"(msr)
                     : /* no clobber */);
}

uint64_t cpu_read_msr(uint32_t msr) {
    uint32_t hi;
    uint32_t lo;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr) : /* no clobber */);
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}
