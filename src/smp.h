#pragma once

void smp_init(void);

[[gnu::noreturn]]
void smp_ap_trampoline_c(void);
