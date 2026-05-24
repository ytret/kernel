#include "arch.h"
#include "assert.h"

#include "arch/x86/arch_smp.h"
#include "arch/x86/gdt.h"

typedef struct [[gnu::packed]] {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} gen_regs_t;

// Defined in taskmgr.s.
extern void taskmgr_switch_tasks(tcb_t *p_from, tcb_t const *p_to,
                                 tss_t *p_tss);
extern void taskmgr_go_usermode_impl(uint32_t user_code_seg,
                                     uint32_t user_data_seg, uint32_t tls_seg,
                                     uint32_t entry_point,
                                     gen_regs_t *p_user_regs);

void arch_taskmgr_local_init(void) {
    // Load the TSS.
    const gdt_seg_sel_t tss_sel = {
        .index = GDT_SMP_TSS_IDX,
        .ti = 0,
        .rpl = 0,
    };
    __asm__ volatile("ltr %%ax"
                     :              /* no outputs */
                     : "a"(tss_sel) /* TSS segment */
                     : /* no clobber */);
}

void arch_taskmgr_switch_tasks(task_t *from, task_t *to) {
    DEBUG_ASSERT(to != NULL);

    smp_proc_t *const proc = smp_get_running_proc();
    DEBUG_ASSERT(proc != NULL);

    arch_smp_proc_t *const arch_proc = proc->arch_ctx;
    DEBUG_ASSERT(arch_proc != NULL);

    if (from) {
        taskmgr_switch_tasks(&from->tcb, &to->tcb, arch_proc->tss);
    } else {
        taskmgr_switch_tasks(NULL, &to->tcb, arch_proc->tss);
    }
}

void arch_taskmgr_go_usermode(uint32_t entry) {
    gen_regs_t gen_regs = {0};
    gen_regs.esp = USER_STACK_TOP;

    taskmgr_go_usermode_impl(0x18, 0x20, 0x28, entry, &gen_regs);
}
