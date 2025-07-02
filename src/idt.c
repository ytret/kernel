#include <stddef.h>

#include "idt.h"
#include "isrs.h"
#include "kprintf.h"
#include "panic.h"
#include "smp.h"
#include "syscall.h"
#include "taskmgr.h"

#define NUM_ENTRIES     256
#define DESC_SIZE_BYTES 6

#define ENTRY_PRESENT (1 << 7)

#define ENTRY_DPL_KERNEL (0 << 5)
#define ENTRY_DPL_USER   (3 << 5)

#define ENTRY_TYPE_INT_32BIT (0xE << 0)

typedef struct [[gnu::packed]] {
    uint16_t offset_15_0;
    uint16_t selector;
    uint8_t reserved;
    uint8_t present_dpl_type;
    uint16_t offset_31_16;
} entry_t;

static uint8_t p_desc[DESC_SIZE_BYTES];
static entry_t gp_idt[NUM_ENTRIES];

static void fill_desc(uint8_t *p_desc, void const *p_idt, uint16_t idt_size);
static void fill_entry(entry_t *p_entry, void (*p_handler)(void));
static void fill_user_entry(entry_t *p_entry, void (*p_handler)(void));

static void print_entry(entry_t const *p_entry);
static void print_stack_frame(isr_stack_frame_t const *p_stack_frame);

void idt_init(void) {
    fill_entry(&gp_idt[0], isr_0);
    fill_entry(&gp_idt[1], isr_1);
    fill_entry(&gp_idt[2], isr_2);
    fill_entry(&gp_idt[3], isr_3);
    fill_entry(&gp_idt[4], isr_4);
    fill_entry(&gp_idt[5], isr_5);
    fill_entry(&gp_idt[6], isr_6);
    fill_entry(&gp_idt[7], isr_7);
    fill_entry(&gp_idt[8], isr_8);
    fill_entry(&gp_idt[9], isr_9);
    fill_entry(&gp_idt[10], isr_10);
    fill_entry(&gp_idt[11], isr_11);
    fill_entry(&gp_idt[12], isr_12);
    fill_entry(&gp_idt[13], isr_13);
    fill_entry(&gp_idt[14], isr_14);
    fill_entry(&gp_idt[15], isr_15);
    fill_entry(&gp_idt[16], isr_16);
    fill_entry(&gp_idt[17], isr_17);
    fill_entry(&gp_idt[18], isr_18);
    fill_entry(&gp_idt[19], isr_19);
    fill_entry(&gp_idt[20], isr_20);
    fill_entry(&gp_idt[21], isr_21);
    fill_entry(&gp_idt[22], isr_22);
    fill_entry(&gp_idt[23], isr_23);
    fill_entry(&gp_idt[24], isr_24);
    fill_entry(&gp_idt[25], isr_25);
    fill_entry(&gp_idt[26], isr_26);
    fill_entry(&gp_idt[27], isr_27);
    fill_entry(&gp_idt[28], isr_28);
    fill_entry(&gp_idt[29], isr_29);
    fill_entry(&gp_idt[30], isr_30);
    fill_entry(&gp_idt[31], isr_31);

    for (size_t idx = 32; idx < NUM_ENTRIES; idx++) {
        fill_entry(&gp_idt[idx], isr_dummy);
    }

    // When adding a new IRQ handler, also update pic_spurious_irq_handler(),
    // so that it calls the new handler too, in case of a spurious IRQ.
    fill_entry(&gp_idt[32 + 0], isr_irq0);
    fill_entry(&gp_idt[32 + 1], isr_irq1);
    fill_entry(&gp_idt[32 + 7], isr_irq7);
    fill_entry(&gp_idt[32 + 15], isr_irq15);

    fill_entry(&gp_idt[SMP_VEC_HALT], isr_ipi_halt);
    fill_entry(&gp_idt[SMP_VEC_TLB_SHOOTDOWN], isr_ipi_tlb_shootdown);

    fill_user_entry(&gp_idt[SYSCALL_INT_NUM], isr_syscall);

    fill_desc(p_desc, gp_idt, (sizeof(gp_idt) - 1));
    idt_load(p_desc);
}

uint8_t *idt_get_desc(void) {
    return p_desc;
}

void idt_dummy_exception_handler(uint32_t exc_num, uint32_t err_code,
                                 isr_stack_frame_t *p_stack_frame) {
    task_t *p_running_task = taskmgr_running_task();

    panic_enter();

    char const *p_name;
    switch (exc_num) {
    case 0:
        p_name = "divide error";
        break;
    case 1:
        p_name = "debug exception";
        break;
    case 2:
        p_name = "nonmaskable interrupt";
        break;
    case 3:
        p_name = "overflow";
        break;
    case 4:
        p_name = "BOUND range exceeded";
        break;
    case 5:
        p_name = "overflow";
        break;
    case 6:
        p_name = "invalid opcode";
        break;
    case 7:
        p_name = "no math coprocessor";
        break;
    case 8:
        p_name = "double fault";
        break;
    case 9:
        p_name = "coprocessor segment overrun";
        break;
    case 10:
        p_name = "invalid TSS";
        break;
    case 11:
        p_name = "segment not present";
        break;
    case 12:
        p_name = "stack segment fault";
        break;
    case 13:
        p_name = "general protection fault";
        break;
    case 14:
        p_name = "page fault";
        break;
    case 15:
        p_name = "reserved";
        break;
    case 16:
        p_name = "FPU floating-point error";
        break;
    case 17:
        p_name = "alignment check";
        break;
    case 18:
        p_name = "machine check";
        break;
    case 19:
        p_name = "SIMD floating-point exception";
        break;
    case 20:
        p_name = "virtualization exception";
        break;
    case 21:
        p_name = "control protection exception";
        break;
    default:
        p_name = "reserved";
    }

    kprintf("Exception: %d (%s)\n", exc_num, p_name);
    if (p_running_task) {
        kprintf("Running task ID: %u\n", p_running_task->id);
    } else {
        kprintf("Running task ID: none\n");
    }
    kprintf("Error code: %d\n", err_code);
    print_stack_frame(p_stack_frame);
    panic("no handler defined");
}

void idt_dummy_handler(isr_stack_frame_t *p_stack_frame) {
    panic_enter();
    kprintf("idt_dummy_handler()\n");
    print_stack_frame(p_stack_frame);
    panic("no handler defined");
}

void idt_page_fault_handler(uint32_t addr, uint32_t err_code,
                            isr_stack_frame_t *p_stack_frame) {
    task_t *p_running_task = taskmgr_running_task();

    panic_enter();
    kprintf("Page fault exception\n");
    if (p_running_task) {
        kprintf("Running task ID: %u\n", p_running_task->id);
    } else {
        kprintf("Running task ID: none\n");
    }
    kprintf("Virtual address: 0x%08X\n", addr);
    kprintf("Error code: %d\n", err_code);
    print_stack_frame(p_stack_frame);
    panic("unresolved page fault");
}

static void fill_desc(uint8_t *p_desc, void const *p_idt, uint16_t idt_size) {
    __builtin_memset(p_desc, 0, DESC_SIZE_BYTES);

    __builtin_memcpy(&p_desc[0], &idt_size, 2);
    __builtin_memcpy(&p_desc[2], &p_idt, 4);
}

static void fill_entry(entry_t *p_entry, void (*p_handler)(void)) {
    __builtin_memset(p_entry, 0, sizeof(*p_entry));

    uint32_t offset = ((uint32_t)p_handler);
    p_entry->offset_15_0 = ((offset >> 0) & 0xFFFF);
    p_entry->offset_31_16 = ((offset >> 16) & 0xFFFF);

    p_entry->selector = 0x08;
    p_entry->present_dpl_type =
        (ENTRY_PRESENT | ENTRY_DPL_KERNEL | ENTRY_TYPE_INT_32BIT);
}

static void fill_user_entry(entry_t *p_entry, void (*p_handler)(void)) {
    __builtin_memset(p_entry, 0, sizeof(*p_entry));

    uint32_t offset = ((uint32_t)p_handler);
    p_entry->offset_15_0 = ((offset >> 0) & 0xFFFF);
    p_entry->offset_31_16 = ((offset >> 16) & 0xFFFF);

    p_entry->selector = 0x08;
    p_entry->present_dpl_type =
        (ENTRY_PRESENT | ENTRY_DPL_USER | ENTRY_TYPE_INT_32BIT);
}

[[maybe_unused]]
static void print_entry(entry_t const *p_entry) {
    kprintf("offset=%P, P=%d, DPL=%d, type=0x%X, selector=0x%X (%X %X)\n",
            ((p_entry->offset_31_16 << 16) | p_entry->offset_15_0),
            ((p_entry->present_dpl_type >> 7) & 1),
            ((p_entry->present_dpl_type >> 5) & 3),
            (p_entry->present_dpl_type & 0xF), p_entry->selector,
            ((uint32_t const *)p_entry)[0], ((uint32_t const *)p_entry)[1]);
}

[[maybe_unused]]
static void print_stack_frame(isr_stack_frame_t const *p_stack_frame) {
    kprintf("Stack frame is at %P:\n", p_stack_frame);
    kprintf("   eip = 0x%X\n", p_stack_frame->eip);
    kprintf("    cs = 0x%X\n", p_stack_frame->cs);
    kprintf("eflags = 0x%X\n", p_stack_frame->eflags);
}
