#include <stdatomic.h>
#include <stdint.h>

#include "gdt.h"
#include "heap.h"
#include "kprintf.h"
#include "panic.h"
#include "pmm.h"
#include "stack.h"
#include "taskmgr.h"
#include "vmm.h"

#define KERNEL_STACK_SIZE 4096

#define USER_STACK_TOP   0x7FFFF000 // must be page-aligned, not checked
#define USER_STACK_PAGES 1

typedef struct __attribute__((packed)) {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} gen_regs_t;

// Nested locks preventing task switching. Initial value of 1 is decremented
// after the scheduler is initialized.
static int g_scheduler_lock = 1;

static list_t g_runnable_tasks;
static task_t *gp_running_task;

// ID for the next new task.
static uint32_t g_new_task_id;

static task_t *new_task(uint32_t entry_point);
static void map_user_stack(uint32_t *p_dir);

// Defined in taskmgr.s.
extern void taskmgr_switch_tasks(tcb_t *p_from, tcb_t const *p_to,
                                 tss_t *p_tss);
extern void taskmgr_go_usermode_impl(uint32_t user_code_seg,
                                     uint32_t user_data_seg, uint32_t tls_seg,
                                     uint32_t entry_point,
                                     gen_regs_t *p_user_regs);

/*
 * Starts the scheduler and causes the initial task to start executing.
 *
 * NOTE: this function does not return, for the initial task entry must not
 * and does not return.
 *
 * NOTE: p_init_entry() must enable interrupts, otherwise no task switches
 * will happen.
 */
__attribute__((noreturn)) void
taskmgr_init(__attribute__((noreturn)) void (*p_init_entry)(void)) {
    // Load the TSS.
    __asm__ volatile("ltr %%ax"
                     :           /* no outputs */
                     : "a"(0x28) /* TSS segment */
                     : /* no clobber */);

    // Critical section.  It ends when the entry is reached.
    __asm__ volatile("cli");

    // Create the initial task.
    gp_running_task = new_task((uint32_t)p_init_entry);

    // If interrupts were enabled and PIT IRQ happened after the following line
    // and before the entry, some bad things would happen to the stack.
    g_scheduler_lock = 0;

    taskmgr_switch_tasks(NULL, &gp_running_task->tcb, gdt_get_tss());

    kprintf("initial task entry has returned\n");
    panic("unexpected behavior");
}

void taskmgr_schedule(void) {
    if (g_scheduler_lock > 0) { return; }

    list_node_t *p_next_node = list_pop_first(&g_runnable_tasks);
    if (!p_next_node) { return; }
    task_t *p_next_task = LIST_NODE_TO_STRUCT(p_next_node, task_t, list_node);

    task_t *p_caller_task = gp_running_task;
    if (!p_caller_task->b_is_blocked) {
        list_append(&g_runnable_tasks, &p_caller_task->list_node);
    }

    gp_running_task = p_next_task;
    taskmgr_switch_tasks(&p_caller_task->tcb, &p_next_task->tcb, gdt_get_tss());
}

void taskmgr_lock_scheduler(void) {
    __asm__ volatile("cli");
    atomic_fetch_add_explicit(&g_scheduler_lock, 1, memory_order_acquire);
}

void taskmgr_unlock_scheduler(void) {
    atomic_fetch_sub_explicit(&g_scheduler_lock, 1, memory_order_release);
    if (atomic_load(&g_scheduler_lock) == 0) { __asm__ volatile("sti"); }
}

task_t *taskmgr_running_task(void) {
    return gp_running_task;
}

list_t *taskmgr_runnable_tasks(void) {
    return &g_runnable_tasks;
}

void taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry) {
    map_user_stack(p_dir);

    task_t *p_task = new_task(entry);
    p_task->tcb.page_dir_phys = ((uint32_t)p_dir);

    taskmgr_lock_scheduler();
    list_append(&g_runnable_tasks, &p_task->list_node);
    taskmgr_unlock_scheduler();
}

void taskmgr_go_usermode(uint32_t entry) {
    gen_regs_t gen_regs = {0};
    gen_regs.esp = USER_STACK_TOP;

    taskmgr_go_usermode_impl(0x18, 0x20, 0x28, entry, &gen_regs);
}

void taskmgr_acquire_mutex(task_mutex_t *p_mutex) {
    if (gp_running_task && taskmgr_owns_mutex(p_mutex)) { panic_silent(); }

    if (__sync_bool_compare_and_swap(&p_mutex->p_locking_task, NULL,
                                     gp_running_task)) {
        // Caller task has successfuly acquired the mutex.
    } else {
        // The mutex is blocked by another task.
        taskmgr_lock_scheduler();
        gp_running_task->b_is_blocked = true;
        list_append(&p_mutex->waiting_tasks, &gp_running_task->list_node);
        taskmgr_unlock_scheduler();

        taskmgr_schedule();
    }
}

void taskmgr_release_mutex(task_mutex_t *p_mutex) {
    taskmgr_lock_scheduler();

    if (gp_running_task && !taskmgr_owns_mutex(p_mutex)) { panic_silent(); }

    list_node_t *p_waiting_task_node = list_pop_first(&p_mutex->waiting_tasks);
    if (gp_running_task && p_waiting_task_node) {
        task_t *p_waiting_task =
            LIST_NODE_TO_STRUCT(p_waiting_task_node, task_t, list_node);
        p_waiting_task->b_is_blocked = false;
        p_mutex->p_locking_task = p_waiting_task;
        list_append(&g_runnable_tasks, &p_waiting_task->list_node);
    } else {
        p_mutex->p_locking_task = NULL;
    }

    taskmgr_unlock_scheduler();
}

bool taskmgr_owns_mutex(task_mutex_t *p_mutex) {
    return p_mutex->p_locking_task == gp_running_task;
}

static task_t *new_task(uint32_t entry_point) {
    task_t *p_task = heap_alloc(sizeof(*p_task));
    __builtin_memset(p_task, 0, sizeof(*p_task));
    p_task->id = (g_new_task_id++);
    kprintf("new_task: p_task = %P id %u\n", p_task, p_task->id);

    // Allocate the kernel stack.
    void *p_stack = heap_alloc(KERNEL_STACK_SIZE);
    stack_new(&p_task->kernel_stack, p_stack, KERNEL_STACK_SIZE);
    kprintf("taskmgr: stack at %P\n", p_stack);

    // Set up the control block.
    p_task->tcb.page_dir_phys = ((uint32_t)vmm_kvas_dir());
    p_task->tcb.p_kernel_stack = &p_task->kernel_stack;

    // Set up initial stack entries that will be popped during a task switch.
    stack_push(&p_task->kernel_stack, entry_point); // eip
    stack_push(&p_task->kernel_stack, 1);           // ebp
    stack_push(&p_task->kernel_stack, 2);           // eax
    stack_push(&p_task->kernel_stack, 3);           // ecx
    stack_push(&p_task->kernel_stack, 4);           // edx
    stack_push(&p_task->kernel_stack, 5);           // ebx
    stack_push(&p_task->kernel_stack, 6);           // esi
    stack_push(&p_task->kernel_stack, 7);           // edi

    return p_task;
}

static void map_user_stack(uint32_t *p_dir) {
    if (USER_STACK_PAGES != 1) {
        kprintf("taskmgr: map_user_stack: USER_STACK_PAGES != 1 is not"
                " implemented\n");
        panic("unimplemented");
    }

    uint32_t phys_page = pmm_pop_page();
    vmm_map_user_page(p_dir, (USER_STACK_TOP - 4096), phys_page);

    // Map a view onto the page for the kernel.
    vmm_map_kernel_page((USER_STACK_TOP - 4096), phys_page);

    // Fill the stack.
    //
    // Do nothing.

    // Unmap the kernel view.
    vmm_unmap_kernel_page(USER_STACK_TOP - 4096);
}
