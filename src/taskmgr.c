/**
 * @file taskmgr.c
 * Task manager implementation.
 */

#include <stdatomic.h>
#include <stdint.h>

#include "assert.h"
#include "cpu.h"
#include "gdt.h"
#include "heap.h"
#include "kprintf.h"
#include "list.h"
#include "memfun.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "smp.h"
#include "stack.h"
#include "taskmgr.h"
#include "vmm.h"

#define KERNEL_STACK_SIZE 4096

/**
 * Userspace stack address (top).
 * @warning
 * The address must be page-aligned.
 */
#define USER_STACK_TOP   0x7FFFF000
/// Size of userspace stacks (pages).
#define USER_STACK_PAGES 1

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

/**
 * Global ID for the next new task.
 * See #task_t.id.
 */
static _Atomic uint32_t g_new_task_id;

/**
 * List of all tasks (node: #task_t.all_tasks_list_node).
 * This list contains the nodes of every task of every processor.
 * - Tasks are added to this list upon creation in #new_task().
 * - Tasks are removed from this list only by the #deleter_task().
 */
static list_t g_taskmgr_all_tasks;

/// Spinlock precluding simultaneous access to #g_taskmgr_all_tasks.
static spinlock_t g_taskmgr_all_tasks_lock;

static void wake_up_sleeping_tasks(void);

static void prv_taskmgr_add_runnable_task(taskmgr_t *taskmgr, task_t *task);
static void prv_taskmgr_add_sleeping_task(taskmgr_t *taskmgr, task_t *task);
static task_t *prv_taskmgr_get_runnable_task(taskmgr_t *taskmgr);

static task_t *new_task(taskmgr_t *taskmgr, uint32_t entry_point);
static void map_user_stack(uint32_t *p_dir);

[[gnu::noreturn]] static void idle_task(void);
[[gnu::noreturn]] static void deleter_task(void);

// Defined in taskmgr.s.
extern void taskmgr_switch_tasks(tcb_t *p_from, tcb_t const *p_to,
                                 tss_t *p_tss);
extern void taskmgr_go_usermode_impl(uint32_t user_code_seg,
                                     uint32_t user_data_seg, uint32_t tls_seg,
                                     uint32_t entry_point,
                                     gen_regs_t *p_user_regs);

void taskmgr_global_init(void) {
    spinlock_init(&g_taskmgr_all_tasks_lock);
}

const list_t *taskmgr_all_tasks_list(void) {
    return &g_taskmgr_all_tasks;
}

void taskmgr_lock_all_tasks_list(void) {
    spinlock_acquire(&g_taskmgr_all_tasks_lock);
}

void taskmgr_unlock_all_tasks_list(void) {
    spinlock_release(&g_taskmgr_all_tasks_lock);
}

[[gnu::noreturn]]
void taskmgr_local_init([[gnu::noreturn]] void (*p_init_entry)(void)) {
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

    // Critical section. The initial entry must enable interrupts.
    __asm__ volatile("cli");

    smp_proc_t *const proc = smp_get_running_proc();
    proc->taskmgr = heap_alloc(sizeof(taskmgr_t));
    kmemset(proc->taskmgr, 0, sizeof(taskmgr_t));
    taskmgr_t *const taskmgr = proc->taskmgr;

    taskmgr->proc_num = proc->proc_num;

    list_init(&taskmgr->runnable_tasks, NULL);
    list_init(&taskmgr->sleeping_tasks, NULL);
    spinlock_init(&taskmgr->runnable_tasks_lock);
    spinlock_init(&taskmgr->sleeping_tasks_lock);

    // Create an idle task.
    taskmgr->idle_task = new_task(taskmgr, (uint32_t)idle_task);
    prv_taskmgr_add_runnable_task(taskmgr, taskmgr->idle_task);

    // Create the deleter task. It is switched to when the running task needs to
    // be terminated.
    taskmgr->deleter_task = new_task(taskmgr, (uint32_t)deleter_task);
    taskmgr->deleter_task->is_blocked = true;

    // Create the initial task.
    taskmgr->init_task = new_task(taskmgr, (uint32_t)p_init_entry);
    taskmgr->running_task = taskmgr->init_task;

    // Initialize the list access spinlocks.
    spinlock_init(&taskmgr->runnable_tasks_lock);
    spinlock_init(&taskmgr->sleeping_tasks_lock);

    // If interrupts were enabled and PIT IRQ happened after the following line
    // and before the entry, some bad things would happen to the stack.
    taskmgr->scheduler_lock = 0;

    taskmgr_switch_tasks(NULL, &taskmgr->running_task->tcb, proc->tss);

    panic_enter();
    kprintf("taskmgr: initial task entry has returned\n");
    panic("unexpected behavior");
}

void taskmgr_local_schedule(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { return; }

    if (taskmgr->scheduler_lock > 0) { return; }

    wake_up_sleeping_tasks();

    task_t *const caller_task = taskmgr->running_task;
    task_t *next_task;
    if (caller_task->is_terminating && !caller_task->is_blocked &&
        caller_task->num_owned_mutexes == 0) {
        next_task = taskmgr->deleter_task;
        taskmgr->task_to_delete = caller_task;

        // NOTE: the deleter task must unlock the scheduler once it's done.
        taskmgr_local_lock_scheduler();
    } else {
        next_task = prv_taskmgr_get_runnable_task(taskmgr);
        if (!next_task) {
            if (caller_task->is_blocked) {
                panic_enter();
                kprintf("No tasks to preempt the blocked running task.\n");
                panic("scheduling failed");
            } else {
                return;
            }
        }

        if (!caller_task->is_blocked && !caller_task->is_sleeping) {
            prv_taskmgr_add_runnable_task(taskmgr, caller_task);
        }
    }

    smp_proc_t *const proc = smp_get_running_proc();
    taskmgr->running_task = next_task;
    taskmgr_switch_tasks(&caller_task->tcb, &next_task->tcb, proc->tss);
}

void taskmgr_local_reschedule(void) {
    bool b_restore_int = false;
    if (cpu_get_int_flag()) {
        b_restore_int = true;
        __asm__ volatile("cli");
    }

    taskmgr_local_schedule();

    if (b_restore_int) { __asm__ volatile("sti"); }
}

void taskmgr_local_lock_scheduler(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }
    taskmgr_lock_scheduler(taskmgr);
}

void taskmgr_local_unlock_scheduler(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }
    taskmgr_unlock_scheduler(taskmgr);
}

task_t *taskmgr_local_running_task(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (taskmgr) {
        return taskmgr->running_task;
    } else {
        return NULL;
    }
}

task_t *taskmgr_local_new_user_task(uint32_t *p_dir, uint32_t entry) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }

    map_user_stack(p_dir);

    task_t *task = new_task(taskmgr, entry);
    task->tcb.page_dir_phys = ((uint32_t)p_dir);

    taskmgr_local_lock_scheduler();
    prv_taskmgr_add_runnable_task(taskmgr, task);
    taskmgr_local_unlock_scheduler();

    return task;
}

task_t *taskmgr_local_new_kernel_task(uint32_t entry) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }

    task_t *task = new_task(taskmgr, entry);
    taskmgr_local_lock_scheduler();
    prv_taskmgr_add_runnable_task(taskmgr, task);
    taskmgr_local_unlock_scheduler();
    return task;
}

void taskmgr_local_go_usermode(uint32_t entry) {
    gen_regs_t gen_regs = {0};
    gen_regs.esp = USER_STACK_TOP;

    taskmgr_go_usermode_impl(0x18, 0x20, 0x28, entry, &gen_regs);
}

void taskmgr_local_sleep_ms(uint32_t duration_ms) {
    taskmgr_t *const taskmgr = smp_get_running_proc()->taskmgr;
    if (!taskmgr) { panic("running processor has no task manager"); }

    if (!taskmgr->running_task) {
        panic_enter();
        kprintf("taskmgr_sleep: no running task\n");
        panic("taskmgr_sleep failed");
    }

    if (!taskmgr->running_task->is_terminating) {
        taskmgr->running_task->sleep_until_counter_ms =
            pit_counter_ms() + duration_ms;

        taskmgr_local_lock_scheduler();
        taskmgr->running_task->is_sleeping = true;
        prv_taskmgr_add_sleeping_task(taskmgr, taskmgr->running_task);
        taskmgr_local_unlock_scheduler();
    }

    taskmgr_local_schedule();
}

void taskmgr_local_terminate_task(task_t *p_task) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }
    if (p_task->taskmgr != taskmgr) {
        panic("tried to delete a non-local task");
    }

    if (p_task == taskmgr->deleter_task) {
        // Deleter task cannot delete itself because:
        // 1) it always marks itself as blocked before rescheduling,
        // 2) it cannot free its own stack.
        panic_enter();
        kprintf("Deleter task (ID %u) cannot delete itself.\n", p_task->id);
        panic("invalid argument");
    }

    p_task->is_terminating = true;
}

void taskmgr_block_running_task(list_t *task_list) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }

    taskmgr_lock_scheduler(taskmgr);
    taskmgr->running_task->is_blocked = true;
    list_append(task_list, &taskmgr->running_task->list_node);
    taskmgr_unlock_scheduler(taskmgr);
}

void taskmgr_lock_scheduler(taskmgr_t *taskmgr) {
    atomic_fetch_add_explicit(&taskmgr->scheduler_lock, 1,
                              memory_order_acquire);
}

void taskmgr_unlock_scheduler(taskmgr_t *taskmgr) {
    atomic_fetch_sub_explicit(&taskmgr->scheduler_lock, 1,
                              memory_order_release);
}

task_t *taskmgr_running_task(taskmgr_t *taskmgr) {
    return taskmgr->running_task;
}

task_t *taskmgr_get_task_by_id(uint32_t task_id) {
    task_t *p_found_task;
    spinlock_acquire(&g_taskmgr_all_tasks_lock);
    LIST_FIND(&g_taskmgr_all_tasks, p_found_task, task_t, all_tasks_list_node,
              p_task->id == task_id, p_task);
    spinlock_release(&g_taskmgr_all_tasks_lock);
    return p_found_task;
}

void taskmgr_unblock(task_t *task) {
    taskmgr_lock_scheduler(task->taskmgr);
    task->is_blocked = false;
    prv_taskmgr_add_runnable_task(task->taskmgr, task);
    taskmgr_unlock_scheduler(task->taskmgr);
}

/**
 * Wakes up sleeping tasks that have slept for the needed amount of time.
 * See #task_t.sleep_until_counter_ms.
 */
static void wake_up_sleeping_tasks(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }

    spinlock_acquire(&taskmgr->sleeping_tasks_lock);

    uint64_t counter_ms = pit_counter_ms();
    list_t sleep_list_copy = taskmgr->sleeping_tasks;
    list_clear(&taskmgr->sleeping_tasks);

    list_node_t *p_node = sleep_list_copy.p_first_node;
    while (p_node != NULL) {
        // Save the next node pointer because list_append() changes it below.
        list_node_t *p_next = p_node->p_next;

        task_t *p_task = LIST_NODE_TO_STRUCT(p_node, task_t, list_node);
        if (p_task->sleep_until_counter_ms <= counter_ms) {
            taskmgr_unblock(p_task);
        } else {
            list_append(&taskmgr->sleeping_tasks, &p_task->list_node);
        }

        p_node = p_next;
    }

    spinlock_release(&taskmgr->sleeping_tasks_lock);
}

static void prv_taskmgr_add_runnable_task(taskmgr_t *taskmgr, task_t *task) {
    spinlock_acquire(&taskmgr->runnable_tasks_lock);
    list_append(&taskmgr->runnable_tasks, &task->list_node);
    spinlock_release(&taskmgr->runnable_tasks_lock);
}

static void prv_taskmgr_add_sleeping_task(taskmgr_t *taskmgr, task_t *task) {
    spinlock_acquire(&taskmgr->sleeping_tasks_lock);
    list_append(&taskmgr->sleeping_tasks, &task->list_node);
    spinlock_release(&taskmgr->sleeping_tasks_lock);
}

static task_t *prv_taskmgr_get_runnable_task(taskmgr_t *taskmgr) {
    task_t *runnable_task = NULL;

    spinlock_acquire(&taskmgr->runnable_tasks_lock);
    list_node_t *const runnable_node = list_pop_first(&taskmgr->runnable_tasks);
    if (runnable_node) {
        runnable_task = LIST_NODE_TO_STRUCT(runnable_node, task_t, list_node);
    }
    spinlock_release(&taskmgr->runnable_tasks_lock);

    return runnable_task;
}

/**
 * Creates a new kernel-mode task with a kernel stack.
 * @param taskmgr     Task manager that will be responsible for the task.
 * @param entry_point Kernel-mode entry point.
 * @returns Task context pointer.
 */
static task_t *new_task(taskmgr_t *taskmgr, uint32_t entry_point) {
    task_t *task = heap_alloc(sizeof(*task));
    __builtin_memset(task, 0, sizeof(*task));
    task->id = (g_new_task_id++);
    task->taskmgr = taskmgr;

    // Allocate the kernel stack.
    void *p_stack = heap_alloc(KERNEL_STACK_SIZE);
    stack_new(&task->kernel_stack, p_stack, KERNEL_STACK_SIZE);

    // Set up the control block.
    task->tcb.page_dir_phys = ((uint32_t)vmm_kvas_dir());
    task->tcb.p_kernel_stack = &task->kernel_stack;

    // Set up initial stack entries that will be popped during a task switch.
    stack_push(&task->kernel_stack, entry_point); // eip
    stack_push(&task->kernel_stack, 1);           // ebp
    stack_push(&task->kernel_stack, 2);           // eax
    stack_push(&task->kernel_stack, 3);           // ecx
    stack_push(&task->kernel_stack, 4);           // edx
    stack_push(&task->kernel_stack, 5);           // ebx
    stack_push(&task->kernel_stack, 6);           // esi
    stack_push(&task->kernel_stack, 7);           // edi

    spinlock_acquire(&g_taskmgr_all_tasks_lock);
    list_append(&g_taskmgr_all_tasks, &task->all_tasks_list_node);
    spinlock_release(&g_taskmgr_all_tasks_lock);

    return task;
}

/**
 * Allocates physical memory for a user stack and maps it.
 * @param p_dir Page directory of the task to create the userspace stack for.
 */
static void map_user_stack(uint32_t *p_dir) {
    if (USER_STACK_PAGES != 1) {
        panic_enter();
        kprintf("taskmgr: map_user_stack: USER_STACK_PAGES != 1 is not"
                " implemented\n");
        panic("unimplemented");
    }

    uint32_t phys_page = pmm_pop_page();
    vmm_map_user_page(p_dir, (USER_STACK_TOP - 4096), phys_page);
    static_assert(USER_STACK_TOP > 4096);
    static_assert(USER_STACK_TOP % 4096 == 0);

    // Map a view onto the page for the kernel.
    vmm_map_kernel_page((USER_STACK_TOP - 4096), phys_page);

    // Fill the stack.
    //
    // Do nothing.

    // Unmap the kernel view.
    vmm_unmap_kernel_page(USER_STACK_TOP - 4096);
}

/**
 * Idle task entry point.
 * See #taskmgr_t.idle_task.
 */
[[gnu::noreturn]]
static void idle_task(void) {
    __asm__ volatile("sti");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/**
 * Deleter task entry point.
 * See #taskmgr_t.deleter_task.
 */
[[gnu::noreturn]]
static void deleter_task(void) {
    taskmgr_t *const taskmgr = smp_get_running_taskmgr();
    if (!taskmgr) { panic("running processor has no task manager"); }

    for (;;) {
        ASSERT(taskmgr->task_to_delete);
        ASSERT(taskmgr->task_to_delete->is_terminating);
        ASSERT(!taskmgr->task_to_delete->is_blocked);
        ASSERT(taskmgr->task_to_delete->num_owned_mutexes == 0);

        heap_free(taskmgr->task_to_delete->kernel_stack.p_bottom);

        if (taskmgr->task_to_delete->tcb.page_dir_phys !=
            (uint32_t)vmm_kvas_dir()) {
            vmm_free_vas(
                (uint32_t *)taskmgr->task_to_delete->tcb.page_dir_phys);
        }

        spinlock_acquire(&g_taskmgr_all_tasks_lock);
        bool removed_task =
            list_remove(&g_taskmgr_all_tasks,
                        &taskmgr->task_to_delete->all_tasks_list_node);
        spinlock_release(&g_taskmgr_all_tasks_lock);
        ASSERT(removed_task);

        heap_free(taskmgr->task_to_delete);
        taskmgr->task_to_delete = NULL;

        // Mark the deleter task as 'blocked' so that it is not added to the
        // list of runnable tasks when it is switched from.
        taskmgr->deleter_task->is_blocked = true;

        taskmgr_local_unlock_scheduler();
        taskmgr_local_schedule();
    }
}
