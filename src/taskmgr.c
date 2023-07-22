#include <gdt.h>
#include <panic.h>
#include <pmm.h>
#include <printf.h>
#include <stack.h>
#include <taskmgr.h>
#include <vmm.h>

#include <stdint.h>

#define KERNEL_STACK_SIZE       4096

#define USER_STACK_TOP          0x7FFFF000 // must be page-aligned, not checked
#define USER_STACK_PAGES        1

typedef struct
__attribute__ ((packed))
{
    uint32_t  page_dir_phys;
    stack_t * p_kernel_stack;
} tcb_t;

typedef struct
{
    uint32_t id;
    stack_t  kernel_stack;

    tcb_t    tcb;
} task_t;

typedef struct
__attribute__ ((packed))
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} gp_regs_t;

static task_t gp_tasks[2];

static uint8_t gpp_kernel_stack[2][KERNEL_STACK_SIZE];

static volatile bool gb_swing;
static volatile bool gb_do_schedule;

static task_t new_task_with_empty_stack(uint32_t task_id);
static task_t new_task_with_stack(uint32_t task_id, uint32_t entry_point);

static void dummy_task_entry(void);
static void map_user_stack(uint32_t * p_dir);

// Defined in taskmgr.s.
//
extern void taskmgr_switch_tasks(tcb_t * p_from, tcb_t const * p_to,
                                 tss_t * p_tss);
extern void taskmgr_go_usermode(uint32_t user_code_seg, uint32_t user_data_seg,
                                uint32_t tls_seg, uint32_t entry_point,
                                gp_regs_t * p_user_regs);

// Defined in usermode.s.
//
static void usermode_entry(void);

void
taskmgr_init (void)
{
    uint32_t init_task_id = 0;

    // The init task is created with an empty kernel stack because it will not
    // be switched onto - it will be switched from, so its context will be
    // pushed, not popped during the next task switch.
    //
    task_t init_task = new_task_with_empty_stack(init_task_id);
    gp_tasks[0] = init_task;

    // The second example task is going to jump to usermode.  We need to create
    // its VAS first, so clone the kernel VAS to begin with.
    //
    uint32_t * p_dir = vmm_clone_kvas();
    printf("taskmgr: cloned kernel VAS at %P\n", p_dir);

    // Set up usermode stack.
    //
    map_user_stack(p_dir);

    task_t dummy_task = new_task_with_stack(1, ((uint32_t) dummy_task_entry));
    gp_tasks[1] = dummy_task;
    gp_tasks[1].tcb.page_dir_phys = ((uint32_t) p_dir);

    // Set up the stacks.
    //
    gp_tasks[0].tcb.p_kernel_stack = &gp_tasks[0].kernel_stack;
    gp_tasks[1].tcb.p_kernel_stack = &gp_tasks[1].kernel_stack;

    // Load the TSS.
    //
    __asm__ volatile ("ltr %%ax"
                      :            /* no outputs */
                      : "a" (0x28) /* TSS segment */
                      :            /* no clobber */);
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
    task.tcb.page_dir_phys = ((uint32_t) vmm_kvas_dir());

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
    // __asm__ volatile ("sti");

    gp_regs_t gp_regs = { 0 };
    gp_regs.esp = USER_STACK_TOP;

    printf("Entering ring 3\n");
    taskmgr_go_usermode(0x18, 0x20, 0, ((uint32_t) usermode_entry), &gp_regs);

    printf("dummy_task_entry: end reached\n");
    panic("unexpected behavior");
}

static void
map_user_stack (uint32_t * p_dir)
{
    if (USER_STACK_PAGES != 1)
    {
        printf("taskmgr: map_user_stack: USER_STACK_PAGES != 1 is not"
               " implemented\n");
        panic("unimplemented");
    }

    uint32_t phys_page = pmm_pop_page();
    vmm_map_user_page(p_dir, (USER_STACK_TOP - 4096), phys_page);

    // Map a view onto the page for the kernel.
    //
    vmm_map_kernel_page((USER_STACK_TOP - 4096), phys_page);

    // Fill the stack.
    //
    // Do nothing.
    //

    // Unmap the kernel view.
    //
    vmm_unmap_kernel_page(USER_STACK_TOP - 4096);
}

static void
usermode_entry (void)
{
    for (;;)
    {}
}
