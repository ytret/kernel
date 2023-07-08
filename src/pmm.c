#include <panic.h>
#include <pmm.h>
#include <printf.h>
#include <stack.h>

#include <limits.h>

#define MMAP_ENTRY_AVAILABLE    1

// See the linker script.
//
extern uint32_t ld_pmm_stack_bottom;
extern uint32_t ld_pmm_stack_top;
extern uint32_t ld_vmm_kernel_end;

static stack_t g_page_stack;

static void parse_mmap(uint32_t addr, uint32_t len);
static void add_region(uint32_t start, uint32_t num_bytes);

static void print_usage(void);

void
pmm_init (mbi_t const * p_mbi)
{
    if (!(p_mbi->flags & MBI_FLAG_MMAP))
    {
        printf("PMM: memory map is not present in multiboot info struct\n");
        panic("memory map unavailable");
    }

    printf("PMM: mmap_length = %d\n", p_mbi->mmap_length);
    printf("PMM: mmap_addr = 0x%X\n", p_mbi->mmap_addr);

    size_t stack_size =
        ((size_t) (((uintptr_t) &ld_pmm_stack_top)
                   - ((uintptr_t) &ld_pmm_stack_bottom)));
    stack_new(&g_page_stack, &ld_pmm_stack_bottom, stack_size);

    parse_mmap(p_mbi->mmap_addr, p_mbi->mmap_length);

    // Print how much of the stack is used.
    //
    print_usage();
}

void
pmm_push_page (uint32_t addr)
{
    if (addr & 0xFFF)
    {
        printf("PMM: cannot push page: addr is not page-aligned\n");
        panic("unexpected behavior");
    }

    if (stack_is_full(&g_page_stack))
    {
        // Cannot push the page - the stack is full.
        //
        return;
    }

    stack_push(&g_page_stack, addr);
}

uint32_t
pmm_pop_page (void)
{
    if (stack_is_empty(&g_page_stack))
    {
        // Cannot pop the page - the stack is empty.
        //
        printf("PMM: cannot pop page: stack is empty\n");
        panic("no free memory");
    }

    return (stack_pop(&g_page_stack));
}

static void
parse_mmap (uint32_t addr, uint32_t map_len)
{
    uint32_t byte = 0;
    while (byte < map_len)
    {
        uint32_t const * p_entry = ((uint32_t const * ) (addr + byte));

        uint32_t size      = *(p_entry + 0);
        uint64_t base_addr = *((uint64_t const *) (p_entry + 1));
        uint64_t length    = *((uint64_t const *) (p_entry + 3));
        uint32_t type      = *(p_entry + 5);

        uint64_t end = (base_addr + length);

        printf("PMM: size = %d, addr = 0x%X, end = 0x%X, type = %d\n",
               size, ((uint32_t) base_addr), ((uint32_t) end), type);

        if ((base_addr > ((uint64_t) UINT_MAX))
            || (length > ((uint64_t) UINT_MAX))
            || (end    > ((uint64_t) UINT_MAX)))
        {
            printf("PMM: region lies outside of 4 GiB memory, ignoring it\n");
        }
        else if (MMAP_ENTRY_AVAILABLE == type)
        {
            if (end < ((uint32_t) &ld_vmm_kernel_end))
            {
                printf("PMM: region is below the kernel, ignoring it\n");
            }
            else
            {
                add_region(((uint32_t) base_addr), ((uint32_t) length));
            }
        }

        byte += (4 + size);
    }
}

static void
add_region (uint32_t start, uint32_t num_bytes)
{
    while (start < ((uint32_t) &ld_vmm_kernel_end))
    {
        start += 4096;
    }

    for (uint32_t page = start; page < (start + num_bytes); page += 4096)
    {
        pmm_push_page(page);
    }

    printf("PMM: added %u bytes starting at 0x%X\n", num_bytes, start);
}

static void
print_usage (void)
{
    uint32_t used_perc =
        (100 * ((uint32_t) (g_page_stack.p_top_max - g_page_stack.p_top))
         / ((uint32_t) (g_page_stack.p_top_max - g_page_stack.p_bottom)));
    printf("PMM: stack is %u%% used", used_perc);

    uint32_t num_bytes =
        (4096 * ((uint32_t) (g_page_stack.p_top_max - g_page_stack.p_top)));
    if (num_bytes <= (10 * 1024 * 1024))
    {
        printf(", holding %u KiB\n", (num_bytes / 1024));
    }
    else
    {
        printf(", holding %u MiB\n", (num_bytes / (1024 * 1024)));
    }
}
