#pragma once

#define SMP_HALT_INT_NUM 0xF1

void smp_init(void);
bool smp_is_active(void);

[[gnu::noreturn]]
void smp_ap_trampoline_c(void);
