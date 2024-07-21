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
    // This field order is relied upon by taskmgr_switch_tasks() assembly
    // function (see taskmgr.s).

    uint32_t page_dir_phys;
    stack_t *p_kernel_stack;
} tcb_t;

typedef struct task {
    uint32_t id;
    stack_t kernel_stack;
    tcb_t tcb;

    struct task *p_next;
} task_t;

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

static bool gb_scheduling;

// First task in the list.
//
// NOTE: this must be the initial task.
static task_t *gp_first_task;

static uint32_t g_new_task_id;
static uint32_t g_running_task_id;

static task_t *new_task(uint32_t entry_point);
static void map_user_stack(uint32_t *p_dir);

static void list_append(task_t *p_first, task_t *p_task);
static size_t list_len(task_t *p_first);
static task_t *list_find(task_t *p_first, uint32_t task_id);

// Defined in taskmgr.s.
extern void taskmgr_switch_tasks(tcb_t *p_from, tcb_t const *p_to,
                                 tss_t *p_tss);
extern void taskmgr_go_usermode_impl(uint32_t user_code_seg,
                                     uint32_t user_data_seg, uint32_t tls_seg,
                                     uint32_t entry_point,
                                     gen_regs_t *p_user_regs);

void taskmgr_init(void) {
    // Load the TSS.
    __asm__ volatile("ltr %%ax"
                     :           /* no outputs */
                     : "a"(0x28) /* TSS segment */
                     : /* no clobber */);
}

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
taskmgr_start_scheduler(__attribute__((noreturn)) void (*p_init_entry)(void)) {
    // Critical section.  It ends when the entry is reached.
    __asm__("cli");

    // If interrupts were disabled and PIT IRQ happened after the following line
    // and before the entry, some bad things would happen to the stack.
    gb_scheduling = true;

    // Create the initial task.
    gp_first_task = new_task((uint32_t)p_init_entry);
    g_running_task_id = gp_first_task->id;

    taskmgr_switch_tasks(NULL, &gp_first_task->tcb, gdt_get_tss());

    kprintf("initial task entry has returned\n");
    panic("unexpected behavior");

    for (;;) {}
}

void taskmgr_schedule(void) {
    if (!gb_scheduling) { return; }

    if (list_len(gp_first_task) < 2) { return; }

    task_t *p_running_task = list_find(gp_first_task, g_running_task_id);
    if (!p_running_task) {
        kprintf("taskmgr: cannot find running task %u in the task list\n",
                g_running_task_id);
        panic("unexpected behavior");
    }

    task_t *p_next_task = p_running_task->p_next;
    if (!p_next_task) { p_next_task = gp_first_task; }

    // Update the running task ID.
    g_running_task_id = p_next_task->id;

    // Switch tasks.
    taskmgr_switch_tasks(&p_running_task->tcb, &p_next_task->tcb,
                         gdt_get_tss());
}

void taskmgr_new_user_task(uint32_t *p_dir, uint32_t entry) {
    // Set up usermode stack.
    map_user_stack(p_dir);

    // Create a task with filled stack that will be popped on the task switch.
    task_t *p_task = new_task(entry);
    p_task->tcb.page_dir_phys = ((uint32_t)p_dir);

    // Add the task to the task list.
    list_append(gp_first_task, p_task);
}

void taskmgr_go_usermode(uint32_t entry) {
    gen_regs_t gen_regs = {0};
    gen_regs.esp = USER_STACK_TOP;

    taskmgr_go_usermode_impl(0x18, 0x20, 0x28, entry, &gen_regs);
}

uint32_t taskmgr_running_task_id(void) {
    return (g_running_task_id);
}

void taskmgr_dump_tasks(void) {
    kprintf(" ID     PAGEDIR         ESP     MAX ESP   USED\n");

    task_t *p_iter = gp_first_task;
    while (p_iter) {
        uint32_t id = p_iter->id;
        uint32_t pagedir = p_iter->tcb.page_dir_phys;
        uint32_t esp = ((uint32_t)p_iter->tcb.p_kernel_stack->p_top);
        uint32_t max_esp = ((uint32_t)p_iter->tcb.p_kernel_stack->p_top_max);
        int32_t used = (max_esp - esp);
        kprintf("%3u  0x%08x  0x%08x  0x%08x  %5d\n", id, pagedir, esp, max_esp,
                used);
        p_iter = p_iter->p_next;
    }
}

static task_t *new_task(uint32_t entry_point) {
    task_t *p_task = heap_alloc(sizeof(*p_task));
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

    return (p_task);
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

static void list_append(task_t *p_first, task_t *p_task) {
    p_task->p_next = NULL;

    if (!p_first) {
        kprintf("taskmgr: list_append: cannot append to empty list\n");
        panic("unexpected behavior");
    }

    // Add to the end.
    task_t *p_iter = p_first;
    while (p_iter->p_next != NULL) {
        p_iter = p_iter->p_next;
    }
    p_iter->p_next = p_task;
}

static size_t list_len(task_t *p_first) {
    size_t len = 0;

    task_t *p_iter = p_first;
    while (p_iter) {
        p_iter = p_iter->p_next;
        len++;
    }

    return (len);
}

static task_t *list_find(task_t *p_first, uint32_t task_id) {
    task_t *p_iter = p_first;
    while (p_iter) {
        if (p_iter->id == task_id) { return (p_iter); }

        p_iter = p_iter->p_next;
    }

    return (NULL);
}
