#pragma once

#include <stdint.h>

#include "smp.h"
#include "types.h"

#define ARCH_VEC_HALT          0xF1 //!< Halt on panic.
#define ARCH_VEC_TLB_SHOOTDOWN 0xF2 //!< TLB shootdown.

// physical/virtual (identity-mapped)
#define ARCH_AP_TRAMPOLINE_ADDR 0x8000
#define ARCH_AP_TRAMPLINE_ARGS  0x8800

// virtual
#define ARCH_AP_INIT_STACK_TOP 0xA000 // page 0x9000..0x9FFF

void arch_early_init(void);
void arch_late_init(void);
void arch_create_platform_tasks(void);

void arch_disable_ints(void);
void arch_enable_ints(void);
void arch_get_ints_enabled(void);

void arch_halt_until_int(void);
void arch_pause_in_loop(void);

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
