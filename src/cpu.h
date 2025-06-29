#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CPU_MSR_APIC_BASE 0x1B

/**
 * APIC Base Address MSR register.
 * Refer to Intel SDM 4, Volume 4, December 2021, Table 2-2 "IA-32 Architectural
 * MSRs", Register Address 1BH.
 */
typedef union [[gnu::packed]] {
    uint64_t val;
    struct [[gnu::packed]] {
        uint64_t reserved1 : 8;
        uint64_t bsp : 1; //!< Boostrap Processor (BSP) flag.
        uint64_t reserved2 : 1;
        uint64_t en_x2_apic : 1; //!< Enable X2 APIC Mode.
        uint64_t apic_gl_en : 1; //!< APIC Global Enable.
        uint64_t apic_base : 48; //!< APIC Base Address.
    } bit;
} cpu_msr_apic_base_t;

uint32_t cpu_get_flags(void);
bool cpu_get_int_flag(void);

void cpu_write_msr(uint32_t msr, uint64_t val);
uint64_t cpu_read_msr(uint32_t msr);
