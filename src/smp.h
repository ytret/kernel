#pragma once

#include <stdint.h>

#define SMP_VEC_HALT          0xF1 //!< Halt on panic.
#define SMP_VEC_TLB_SHOOTDOWN 0xF2 //!< TLB shootdown.

void smp_init(void);
bool smp_is_active(void);

void smp_send_tlb_shootdown(uint32_t addr);
void smp_tlb_shootdown_handler(void);

[[gnu::noreturn]]
void smp_ap_trampoline_c(void);
