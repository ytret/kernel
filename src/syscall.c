#include "kprintf.h"
#include "syscall.h"

void syscall_dispatch(isr_regs_t *p_regs) {
    kprintf("Syscall\n");
    kprintf("p_regs->eax = %u\n", p_regs->eax);
    kprintf("p_regs->ecx = %u\n", p_regs->ecx);
    kprintf("p_regs->edx = %u\n", p_regs->edx);
    kprintf("p_regs->ebx = %u\n", p_regs->ebx);
    kprintf("p_regs->esi = %u\n", p_regs->esi);
    kprintf("p_regs->edi = %u\n", p_regs->edi);
}
