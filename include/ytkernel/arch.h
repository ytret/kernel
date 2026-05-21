#pragma once

#include <stdint.h>

#include "types.h"


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
