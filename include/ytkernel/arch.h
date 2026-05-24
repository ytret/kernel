#pragma once

#include <stdint.h>

#include "pmm.h"
#include "smp.h"
#include "textdisp.h"
#include "types.h"

#ifdef YTKERNEL_ARCH_X86
#include "arch/x86/arch_defs.h" // IWYU pragma: export
#else
#error "Please update include/ytkernel/arch.h to include arch_defs.h"
#endif

typedef struct isr_regs isr_regs_t;

void arch_early_init(pmm_mmap_t *mmap);
void arch_early_init_heap(void);
void arch_late_init(void);
void arch_create_platform_tasks(void);

const char *arch_get_cmdline(void);
paddr_t arch_get_kernel_phys_end(void);

void arch_textdisp_early_init(textdisp_t *disp);

void arch_halt_until_int(void);
void arch_pause_in_loop(void);

void arch_disable_ints(void);
void arch_enable_ints(void);
void arch_get_ints_enabled(void);
void arch_ack_int(void);
void arch_map_irq(uint32_t irq, uint32_t vec);

void arch_send_ipi(uint8_t proc_num, uint8_t vector);
void arch_broadcast_ipi(uint8_t vector);
void arch_ack_ipi(void);

void arch_flush_tlb_addr(vaddr_t addr);
void arch_flush_tlb(void);

size_t arch_smp_num_procs(void);
void arch_smp_init_proc_ctx(smp_proc_t *proc);
void arch_smp_init_ap(smp_proc_t *proc);
void arch_smp_init_bsp(void);
smp_proc_t *arch_smp_get_running_proc(void);
[[gnu::noreturn]] void arch_smp_ap_trampoline_c(void);

void arch_taskmgr_local_init(void);
void arch_taskmgr_switch_tasks(task_t *from, task_t *to);
void arch_taskmgr_go_usermode(uint32_t entry);

[[gnu::noreturn]] void arch_init_bsp_task(void);
[[gnu::noreturn]] void arch_init_ap_task(void);

size_t arch_walk_stack(vaddr_t *arr_addr, size_t max_items, vaddr_t init_sp);

void arch_syscall_dispatch(isr_regs_t *regs);
