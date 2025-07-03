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

/**
 * @{
 * @anchor smp_sync
 * @name Processor initialization synchronization functions
 *
 * Due to the blocking nature of mutexes, which are used in various sensitive
 * kernel modules (e.g., #g_kvas_lock, #g_heap_mutex), there must not be a state
 * where only _some_ of the processors have a running task manager. Otherwise,
 * if #mutex_acquire() sees that there is no task manager on the running
 * processor, but the mutex is locked by some task, it panics.
 *
 * These functions allow the processors to synchronize, i.e. do nothing, until
 * all processors have a running task manager.
 */
/// Returns `true` if the BSP initial task has been reached.
bool smp_is_bsp_ready(void);

/**
 * Indicates to the APs that the BSP has reached the initial task.
 * See #smp_is_bsp_ready().
 * @warning
 * Use this only in the BSP initital task, see #init_bsp_task().
 */
void smp_set_bsp_ready(void);

/**
 * Indicates to the BSP that the running AP has reached the initial task.
 * @warning
 * Use this only in the AP initial task, see #init_ap_task().
 */
void smp_set_ap_ready(void);
/// @}

uint8_t smp_get_num_procs(void);
smp_proc_t *smp_get_proc(uint8_t proc_num);
smp_proc_t *smp_get_running_proc(void);
taskmgr_t *smp_get_running_taskmgr(void);

void smp_init_proc_taskmgr(void);

void smp_send_tlb_shootdown(uint32_t addr);
void smp_tlb_shootdown_handler(void);

[[gnu::noreturn]]
void smp_ap_trampoline_c(void);
