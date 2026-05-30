#include "isrs.h"
#include "ksyscall.h"
#include "ksyscall_arch.h"

int ksyscall_arch_dispatch(void *v_regs) {
    isr_regs_t *const regs = v_regs;
    ksyscall_args_t args = {
        .call_id = regs->eax,
        .arg1 = regs->ecx,
        .arg2 = regs->edx,
        .arg3 = regs->ebx,
        .arg4 = regs->esi,
        .arg5 = regs->edi,
    };
    return ksyscall_dispatch(&args);
}
