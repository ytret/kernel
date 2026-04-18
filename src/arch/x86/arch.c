#include "arch.h"
#include "arch/x86/gdt.h"

void arch_init_1(void) {
    gdtr_t gdtr;
    gdt_init_pre_smp(&gdtr);
    gdt_load(&gdtr);
}
