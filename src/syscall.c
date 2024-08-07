#include "kprintf.h"
#include "syscall.h"

void syscall_dispatch(isr_regs_t *p_regs) {
    kprintf("Syscall\n"
            "p_regs->eax = %u\n"
            "p_regs->ecx = %u\n"
            "p_regs->edx = %u\n"
            "p_regs->ebx = %u\n"
            "p_regs->esi = %u\n"
            "p_regs->edi = %u\n",
            p_regs->eax, p_regs->ecx, p_regs->edx, p_regs->ebx, p_regs->esi,
            p_regs->edi);
}
