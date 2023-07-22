#include <alloc.h>
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

typedef struct task
{
    uint32_t id;
    stack_t  kernel_stack;
    tcb_t    tcb;

    struct task * p_next;
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

static bool gb_initialized;

static task_t * gp_first_task;
static uint32_t g_new_task_id;
static uint32_t g_running_task_id;

static task_t new_task_with_no_stack(uint32_t task_id);
static task_t new_task_with_stack(uint32_t task_id, uint32_t entry_point);

static void map_user_stack(uint32_t * p_dir);

static void     list_append(task_t * p_first, task_t task);
static size_t   list_len(task_t * p_first);
static task_t * list_find(task_t * p_first, uint32_t task_id);

// Defined in taskmgr.s.
//
extern void taskmgr_switch_tasks(tcb_t * p_from, tcb_t const * p_to,
                                 tss_t * p_tss);
extern void taskmgr_go_usermode(uint32_t user_code_seg, uint32_t user_data_seg,
                                uint32_t tls_seg, uint32_t entry_point,
                                gp_regs_t * p_user_regs);

void
taskmgr_init (void)
{
    task_t * p_init_task = alloc(sizeof(*p_init_task));
    *p_init_task = new_task_with_no_stack(g_new_task_id++);

    p_init_task->kernel_stack.p_top_max = ((void *) 0x10b000);
    p_init_task->kernel_stack.p_bottom  = ((void *) 0x107000);

    // Add the init task to the list.
    //
    gp_first_task = p_init_task;

    g_running_task_id = p_init_task->id;

    // Load the TSS.
    //
    __asm__ volatile ("ltr %%ax"
                      :            /* no outputs */
                      : "a" (0x28) /* TSS segment */
                      :            /* no clobber */);

    gb_initialized = true;
}

void
taskmgr_new_user_task (uint32_t * p_dir, uint32_t entry)
{
    // Set up usermode stack.
    //
    map_user_stack(p_dir);

    // Create a task with filled stack that will be popped on the task switch.
    //
    task_t task = new_task_with_stack(g_new_task_id++, entry);
    task.tcb.page_dir_phys = ((uint32_t) p_dir);

    // Add the task to the task list.
    //
    list_append(gp_first_task, task);
}

void
taskmgr_schedule (void)
{
    if (!gb_initialized)
    {
        return;
    }

    if (list_len(gp_first_task) < 2)
    {
        return;
    }

    task_t * p_running_task = list_find(gp_first_task, g_running_task_id);
    if (!p_running_task)
    {
        printf("taskmgr: cannot find running task %u in the task list\n",
               g_running_task_id);
        panic("unexpected behavior");
    }

    task_t * p_next_task = p_running_task->p_next;
    if (!p_next_task)
    {
        p_next_task = gp_first_task;
    }

    // Update tasks' TCBs.
    //
    p_running_task->tcb.p_kernel_stack = &p_running_task->kernel_stack;
    p_next_task->tcb.p_kernel_stack = &p_next_task->kernel_stack;

    // Update the running task ID.
    //
    g_running_task_id = p_next_task->id;

    // Switch tasks.
    //
    taskmgr_switch_tasks(&p_running_task->tcb, &p_next_task->tcb,
                         gdt_get_tss());
}

uint32_t
taskmgr_running_task_id (void)
{
    return (g_running_task_id);
}

void
taskmgr_dump_tasks (void)
{
    printf(" ID     PAGEDIR\n");

    task_t * p_iter = gp_first_task;
    while (p_iter)
    {
        printf("%3u  0x%08x\n", p_iter->id, p_iter->tcb.page_dir_phys);
        p_iter = p_iter->p_next;
    }
}

static task_t
new_task_with_no_stack (uint32_t task_id)
{
    task_t task = {
        .id = task_id,
    };

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
    task_t task = new_task_with_no_stack(task_id);

    // Set up the stack.
    //
    void * p_stack = alloc(KERNEL_STACK_SIZE);
    stack_new(&task.kernel_stack, p_stack, KERNEL_STACK_SIZE);

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

    // Unmap the kernel view.
    //
    vmm_unmap_kernel_page(USER_STACK_TOP - 4096);
}

static void
list_append (task_t * p_first, task_t task)
{
    task_t * p_task = alloc(sizeof(task_t));
    *p_task = task;
    p_task->p_next = NULL;

    if (!p_first)
    {
        printf("taskmgr: list_append: cannot append to empty list\n");
        panic("unexpected behavior");
    }

    // Add to the end.
    //
    task_t * p_iter = p_first;
    while (p_iter->p_next != NULL)
    {
        p_iter = p_iter->p_next;
    }
    p_iter->p_next = p_task;
}

static size_t
list_len (task_t * p_first)
{
    size_t len = 0;

    task_t * p_iter = p_first;
    while (p_iter)
    {
        p_iter = p_iter->p_next;
        len++;
    }

    return (len);
}

static task_t *
list_find (task_t * p_first, uint32_t task_id)
{
    task_t * p_iter = p_first;
    while (p_iter)
    {
        if (p_iter->id == task_id)
        {
            return (p_iter);
        }

        p_iter = p_iter->p_next;
    }

    return (NULL);
}
