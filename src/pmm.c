#include <limits.h>

#include "heap.h"
#include "kprintf.h"
#include "panic.h"
#include "pmm.h"
#include "stack.h"

// First free page.
//
// Any page below this address will not be pushed onto the stack.  It is either
// the kernel end, if there are no modules, or the last module end.
static uint32_t g_first_free_page;

// Variables from the linker script.
extern uint32_t ld_pmm_stack_bottom;
extern uint32_t ld_pmm_stack_top;
extern uint32_t ld_vmm_kernel_end;

static stack_t g_page_stack;

static void prv_pmm_add_region(uint32_t start, uint32_t num_bytes);
static void prv_pmm_print_usage(void);

void pmm_init(const pmm_mmap_t *mmap) {
    kprintf("pmm: memory map entries: %d\n", mmap->num_entries);

    // Determine the first free page.
    g_first_free_page = ((heap_end() + 0xFFF) & ~0xFFF);
    kprintf("pmm: first free page: %P\n", g_first_free_page);

    // Prepare the free page stack.
    size_t stack_size = ((size_t)(((uintptr_t)&ld_pmm_stack_top) -
                                  ((uintptr_t)&ld_pmm_stack_bottom)));
    stack_new(&g_page_stack, &ld_pmm_stack_bottom, stack_size);

    // Parse the memory map, pushing available pages onto the stack.
    for (size_t idx = 0; idx < mmap->num_entries; idx++) {
        const pmm_region_t *const region = &mmap->entries[idx];

        if (region->start > UINT32_MAX) { continue; }
        if (region->end_incl > UINT32_MAX) { continue; }

        kprintf("pmm: region 0x%08X..0x%08X, type = %d\n",
                (uint32_t)region->start, (uint32_t)region->end_incl,
                region->type);

        if (region->type != PMM_REGION_AVAILABLE) { continue; }

        if (region->end_incl >= g_first_free_page) {
            uint32_t start = region->start;
            uint32_t len = region->end_incl - region->start + 1;
            while (start < g_first_free_page) {
                start += 4096;
                len -= 4096;
            }

            prv_pmm_add_region((uint32_t)region->start, len);
        }
    }

    // Print how much of the stack is used.
    prv_pmm_print_usage();
}

void pmm_push_page(uint32_t addr) {
    if (addr & 0xFFF) {
        panic_enter();
        kprintf("pmm: cannot push page: addr is not page-aligned\n");
        panic("unexpected behavior");
    }

    if (stack_is_full(&g_page_stack)) {
        // Cannot push the page - the stack is full.
        return;
    }

    stack_push(&g_page_stack, addr);
}

uint32_t pmm_pop_page(void) {
    if (stack_is_empty(&g_page_stack)) {
        // Cannot pop the page - the stack is empty.
        panic_enter();
        kprintf("pmm: cannot pop page: stack is empty\n");
        panic("no free memory");
    }

    return stack_pop(&g_page_stack);
}

static void prv_pmm_add_region(uint32_t start, uint32_t num_bytes) {
    for (uint32_t page = start; page < (start + num_bytes); page += 4096) {
        pmm_push_page(page);
    }

    kprintf("pmm: added %u bytes starting at 0x%X\n", num_bytes, start);
}

static void prv_pmm_print_usage(void) {
    uint32_t used_perc =
        (100 * ((uint32_t)(g_page_stack.p_top_max - g_page_stack.p_top)) /
         ((uint32_t)(g_page_stack.p_top_max - g_page_stack.p_bottom)));
    kprintf("pmm: stack is %u%% used", used_perc);

    uint32_t num_bytes =
        (4096 * ((uint32_t)(g_page_stack.p_top_max - g_page_stack.p_top)));
    if (num_bytes < 10 * 1024) {
        kprintf(", holding %u bytes\n", num_bytes);
    } else if (num_bytes <= (10 * 1024 * 1024)) {
        kprintf(", holding %u KiB\n", (num_bytes / 1024));
    } else {
        kprintf(", holding %u MiB\n", (num_bytes / (1024 * 1024)));
    }
}
