#include <stdatomic.h>
#include <stdint.h>

#include "assert.h"
#include "gdt.h"
#include "heap.h"
#include "kprintf.h"
#include "list.h"
#include "panic.h"
#include "pit.h"
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

static task_t *gp_running_task;
static list_t g_runnable_tasks;
static list_t g_sleeping_tasks;
static list_t g_all_tasks;

static task_t *gp_deleter_task;
static task_t *gp_task_to_delete;

// ID for the next new task.
static uint32_t g_new_task_id;

static void wake_up_sleeping_tasks(void);

static task_t *new_task(uint32_t entry_point);
static void map_user_stack(uint32_t *p_dir);

static void idle_task(void) __attribute__((noreturn));
static void deleter_task(void) __attribute__((noreturn));

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

    // Create an idle task.
    task_t *p_idle_task = new_task((uint32_t)idle_task);
    list_append(&g_runnable_tasks, &p_idle_task->list_node);

    // Create the deleter task. It is switched to when the running task needs to
    // be terminated.
    gp_deleter_task = new_task((uint32_t)deleter_task);

    // Create the initial task.
    gp_running_task = new_task((uint32_t)p_init_entry);

    // If interrupts were enabled and PIT IRQ happened after the following line
    // and before the entry, some bad things would happen to the stack.
    g_scheduler_lock = 0;

    taskmgr_switch_tasks(NULL, &gp_running_task->tcb, gdt_get_tss());

    panic_enter();
    kprintf("initial task entry has returned\n");
    panic("unexpected behavior");
}

void taskmgr_schedule(void) {
    if (g_scheduler_lock > 0) { return; }

    wake_up_sleeping_tasks();

    task_t *p_caller_task = gp_running_task;
    task_t *p_next_task;
    if (p_caller_task->b_is_terminating &&
        p_caller_task->num_owned_mutexes == 0) {
        p_next_task = gp_deleter_task;
        gp_task_to_delete = p_caller_task;
    } else {
        list_node_t *p_next_node = list_pop_first(&g_runnable_tasks);
        if (!p_next_node) { return; }
        p_next_task = LIST_NODE_TO_STRUCT(p_next_node, task_t, list_node);
    }

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

list_t *taskmgr_all_tasks_list(void) {
    return &g_all_tasks;
}

task_t *taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry) {
    map_user_stack(p_dir);

    task_t *p_task = new_task(entry);
    p_task->tcb.page_dir_phys = ((uint32_t)p_dir);

    taskmgr_lock_scheduler();
    list_append(&g_runnable_tasks, &p_task->list_node);
    taskmgr_unlock_scheduler();

    return p_task;
}

void taskmgr_go_usermode(uint32_t entry) {
    gen_regs_t gen_regs = {0};
    gen_regs.esp = USER_STACK_TOP;

    taskmgr_go_usermode_impl(0x18, 0x20, 0x28, entry, &gen_regs);
}

void taskmgr_sleep_ms(uint32_t duration_ms) {
    if (!gp_running_task) {
        panic_enter();
        kprintf("taskmgr_sleep: no running task\n");
        panic("taskmgr_sleep failed");
    }

    gp_running_task->sleep_until_counter_ms = pit_counter_ms() + duration_ms;

    taskmgr_lock_scheduler();
    taskmgr_block_running_task(&g_sleeping_tasks);
    taskmgr_unlock_scheduler();

    taskmgr_schedule();
}

void taskmgr_terminate_task(task_t *p_task) {
    p_task->b_is_terminating = true;
}

void taskmgr_block_running_task(list_t *p_task_list) {
    taskmgr_lock_scheduler();
    gp_running_task->b_is_blocked = true;
    list_append(p_task_list, &gp_running_task->list_node);
    taskmgr_unlock_scheduler();
}

void taskmgr_unblock(task_t *p_task) {
    taskmgr_lock_scheduler();
    p_task->b_is_blocked = false;
    list_append(&g_runnable_tasks, &p_task->list_node);
    taskmgr_unlock_scheduler();
}

static void wake_up_sleeping_tasks(void) {
    uint64_t counter_ms = pit_counter_ms();
    list_t sleep_list_copy = g_sleeping_tasks;
    list_clear(&g_sleeping_tasks);

    list_node_t *p_node = sleep_list_copy.p_first_node;
    while (p_node != NULL) {
        // Save the next node pointer because list_append() changes it below.
        list_node_t *p_next = p_node->p_next;

        task_t *p_task = LIST_NODE_TO_STRUCT(p_node, task_t, list_node);
        if (p_task->sleep_until_counter_ms <= counter_ms) {
            taskmgr_unblock(p_task);
        } else {
            list_append(&g_sleeping_tasks, &p_task->list_node);
        }

        p_node = p_next;
    }
}

static task_t *new_task(uint32_t entry_point) {
    task_t *p_task = heap_alloc(sizeof(*p_task));
    __builtin_memset(p_task, 0, sizeof(*p_task));
    p_task->id = (g_new_task_id++);

    // Allocate the kernel stack.
    void *p_stack = heap_alloc(KERNEL_STACK_SIZE);
    stack_new(&p_task->kernel_stack, p_stack, KERNEL_STACK_SIZE);

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

    list_append(&g_all_tasks, &p_task->all_tasks_list_node);

    return p_task;
}

static void map_user_stack(uint32_t *p_dir) {
    if (USER_STACK_PAGES != 1) {
        panic_enter();
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

__attribute__((noreturn)) static void idle_task(void) {
    __asm__ volatile("sti");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

__attribute__((noreturn)) static void deleter_task(void) {
    for (;;) {
        taskmgr_lock_scheduler();
        ASSERT(gp_task_to_delete);
        ASSERT(gp_task_to_delete->b_is_terminating);
        ASSERT(!gp_task_to_delete->b_is_blocked);
        ASSERT(gp_task_to_delete->num_owned_mutexes == 0);

        heap_free(gp_task_to_delete->kernel_stack.p_bottom);

        if (gp_task_to_delete->tcb.page_dir_phys != (uint32_t)vmm_kvas_dir()) {
            vmm_free_vas((uint32_t *)gp_task_to_delete->tcb.page_dir_phys);
        }

        ASSERT(
            list_remove(&g_all_tasks, &gp_task_to_delete->all_tasks_list_node));

        heap_free(gp_task_to_delete);
        gp_task_to_delete = NULL;

        // Mark the deleter task as 'blocked' so that it is not added to the
        // list of runnable tasks when it is switched from.
        gp_deleter_task->b_is_blocked = true;

        taskmgr_unlock_scheduler();
        taskmgr_schedule();
    }
}
