#pragma once

#include <stdint.h>

#include "acpi/acpi.h"
#include "gdt.h"
#include "taskmgr.h"

#define SMP_VEC_HALT          0xF1 //!< Halt on panic.
#define SMP_VEC_TLB_SHOOTDOWN 0xF2 //!< TLB shootdown.

typedef struct {
    uint8_t proc_num;
    const acpi_proc_t *acpi;
    gdt_seg_desc_t *gdt;
    tss_t *tss;
    gdtr_t gdtr;
    taskmgr_t *taskmgr;
} smp_proc_t;

void smp_init(void);
bool smp_is_active(void);

bool smp_is_bsp_ready(void);
void smp_set_bsp_ready(void);
void smp_set_ap_ready(void);

uint8_t smp_get_num_procs(void);
smp_proc_t *smp_get_proc(uint8_t proc_num);
smp_proc_t *smp_get_running_proc(void);
taskmgr_t *smp_get_running_taskmgr(void);

void smp_init_proc_taskmgr(void);

void smp_send_tlb_shootdown(uint32_t addr);
void smp_tlb_shootdown_handler(void);

[[gnu::noreturn]]
void smp_ap_trampoline_c(void);
