#include <gdt.h>
#include <printf.h>
#include <stack.h>
#include <taskmgr.h>

#include <stdint.h>

#define KERNEL_STACK_SIZE       512

typedef struct
{
    uint32_t id;
    stack_t  kernel_stack;

    tcb_t    tcb;
} task_t;

static task_t gp_tasks[2];

static uint8_t gpp_kernel_stack[2][KERNEL_STACK_SIZE];

static volatile bool gb_swing;

static task_t new_task_with_empty_stack(uint32_t task_id);
static task_t new_task_with_stack(uint32_t task_id, uint32_t entry_point);

static void dummy_task_entry(void);

static volatile bool gb_do_schedule;

void
taskmgr_init (void)
{
    // tss_t * p_tss = gdt_get_tss();

    uint32_t init_task_id = 0;

    // The init task is created with an empty kernel stack because it will not
    // be switched onto - it will be switched from, so its context will be
    // pushed, not popped during the next task switch.
    //
    task_t init_task = new_task_with_empty_stack(init_task_id);

    // Load the TSS.
    //
    __asm__ volatile ("ltr %%ax"
                      :           /* no outputs */
                      : "a"(0x28) /* TSS segment */
                      :           /* no clobber */);

    gp_tasks[0] = init_task;

    task_t dummy_task = new_task_with_stack(1, ((uint32_t) dummy_task_entry));
    gp_tasks[1] = dummy_task;

    // Set up the stacks.
    //
    gp_tasks[0].tcb.p_kernel_stack = &gp_tasks[0].kernel_stack;
    gp_tasks[1].tcb.p_kernel_stack = &gp_tasks[1].kernel_stack;

    gb_do_schedule = true;

    for (;;)
    {
        printf("?");
        for (int i = 0; i < 10000000; i++)
        {}
    }
}

void
taskmgr_schedule (void)
{
    if (!gb_do_schedule)
    {
        return;
    }

    int from, to;

    if (!gb_swing)
    {
        from = 0;
        to = 1;
    }
    else
    {
        from = 1;
        to = 0;
    }

    gb_swing = !gb_swing;

    taskmgr_switch_tasks(&gp_tasks[from].tcb, &gp_tasks[to].tcb,
                         gdt_get_tss());
}

static task_t
new_task_with_empty_stack (uint32_t task_id)
{
    task_t task = {
        .id = task_id,
    };

    // Set up the stack.
    //
    stack_new(&task.kernel_stack, gpp_kernel_stack[task_id], KERNEL_STACK_SIZE);

    // Create the control block.
    //
    task.tcb.page_dir_phys = 0;

    // tcb.p_kernel_stack is not initialized here, because it would point to a
    // local variable task.kernel_stack.  Instead, it is initialized right
    // before task switching.

    return (task);
}

static task_t
new_task_with_stack (uint32_t task_id, uint32_t entry_point)
{
    task_t task = new_task_with_empty_stack(task_id);

    // Set up an initial stack that will be popped during a task switch.
    //
    stack_push(&task.kernel_stack, entry_point); // eip
    stack_push(&task.kernel_stack, 1); // ebp
    stack_push(&task.kernel_stack, 2); // eax
    stack_push(&task.kernel_stack, 3); // ecx
    stack_push(&task.kernel_stack, 4); // edx
    stack_push(&task.kernel_stack, 5); // ebx
    stack_push(&task.kernel_stack, 6); // esi
    stack_push(&task.kernel_stack, 7); // edi

    return (task);
}

static void
dummy_task_entry (void)
{
    __asm__ volatile ("sti");

    for (;;)
    {
        printf("!");
        for (int i = 0; i < 10000000; i++)
        {}
    }
}
