#include <cpuid.h>
#include <stddef.h>
#include <stdint.h>

#include "arch.h"
#include "heap.h"
#include "kspinlock.h"
#include "memfun.h"
#include "panic.h"
#include "smp.h"

typedef struct {
    uint32_t addr;
    _Atomic int ack_count;
} smp_tlb_shootdown_req_t;

static bool g_smp_is_active;

/**
 * Flag: BSP has finished initializing the APs and reached the initial task.
 */
static _Atomic bool g_smp_bsp_done;

/**
 * Flag: the AP currently being initialized has reached the initial task.
 */
static _Atomic bool g_smp_curr_ap_done;

static smp_proc_t *g_smp_procs;
static _Atomic uint8_t g_smp_num_procs;

/**
 * The number of fully initialized processors, including the BSP.
 *
 * See #smp_send_tlb_shootdown().
 */
static uint8_t g_smp_num_init_procs;

static spinlock_t g_smp_tlb_shootdown_lock;
static smp_tlb_shootdown_req_t g_smp_tlb_shootdown_req;

void smp_init(void) {
    // The BSP is already initialized.
    g_smp_num_init_procs = 1;

    spinlock_init(&g_smp_tlb_shootdown_lock);

    const uint8_t num_procs = arch_smp_num_procs();

    // Allocate the processor context array for the total number of processors.
    // Some of them may be unusable, but for simplicity we allocate for them,
    // too.
    g_smp_procs = heap_alloc(num_procs * sizeof(smp_proc_t));

    // If there are more than 1 usable processors, we consider the SMP to be
    // "active". The most important effect is that entering a kernel panic on
    // one of the processors causes a "halt" IPI to be sent by the panicking
    // processor. See panic.c.
    if (num_procs > 1) { g_smp_is_active = true; }

    for (uint8_t proc_num = 0; proc_num < num_procs; proc_num++) {
        smp_proc_t *const proc = &g_smp_procs[proc_num];
        proc->proc_num = proc_num;

        arch_smp_init_proc_ctx(proc);
        g_smp_num_procs++;

        if (!proc->is_bsp) { arch_smp_init_ap(proc); }

        // FIXME: skip disabled processors.
    }

    // Before?
    arch_smp_init_bsp();
}

bool smp_is_active(void) {
    return g_smp_is_active;
}

bool smp_is_bsp_ready(void) {
    return g_smp_bsp_done;
}

void smp_set_bsp_ready(void) {
    g_smp_bsp_done = true;
}

void smp_reset_ap_ready(void) {
    g_smp_curr_ap_done = false;
}

void smp_set_ap_ready(void) {
    g_smp_num_init_procs += 1;
    g_smp_curr_ap_done = true;
}

bool smp_is_ap_ready(void) {
    return g_smp_curr_ap_done;
}

uint8_t smp_get_num_procs(void) {
    return g_smp_num_procs;
}

smp_proc_t *smp_get_proc(uint8_t proc_num) {
    if (g_smp_procs && proc_num < g_smp_num_procs) {
        return &g_smp_procs[proc_num];
    } else {
        return NULL;
    }
}

smp_proc_t *smp_get_running_proc(void) {
    return arch_smp_get_running_proc();
}

taskmgr_t *smp_get_running_taskmgr(void) {
    smp_proc_t *proc = smp_get_running_proc();
    if (proc) {
        return proc->taskmgr;
    } else {
        return NULL;
    }
}

void smp_init_proc_taskmgr(void) {
    taskmgr_t *const taskmgr = heap_alloc(sizeof(*taskmgr));
    kmemset(taskmgr, 0, sizeof(*taskmgr));
}

void smp_send_tlb_shootdown(uint32_t addr) {
    spinlock_acquire(&g_smp_tlb_shootdown_lock);

    g_smp_tlb_shootdown_req.addr = addr;
    g_smp_tlb_shootdown_req.ack_count = 0;

    arch_broadcast_ipi(ARCH_VEC_TLB_SHOOTDOWN);

    const uint8_t num_procs = g_smp_num_init_procs;
    while (g_smp_tlb_shootdown_req.ack_count < num_procs - 1) {
        arch_pause_in_loop();
    }

    spinlock_release(&g_smp_tlb_shootdown_lock);
}

void smp_tlb_shootdown_handler(void) {
    arch_flush_tlb_addr(g_smp_tlb_shootdown_req.addr);
    g_smp_tlb_shootdown_req.ack_count++;
    arch_ack_ipi();
}

[[gnu::noreturn]]
void smp_ap_trampoline_c(void) {
    // NOTE: this function is called from arch_smp_ap_trampoline().

    taskmgr_local_init(arch_init_ap_task);

    PANIC("end of smp_ap_trampoline_c");
}
