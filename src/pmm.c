#include <panic.h>
#include <pmm.h>
#include <printf.h>

#include <limits.h>

#define MMAP_ENTRY_AVAILABLE    1

// See the linker script.
//
extern uint32_t ld_pmm_stack_bottom;
extern uint32_t ld_pmm_stack_top;

static uint32_t * gp_stack_bottom;
static uint32_t * gp_stack_top_max;
static volatile uint32_t * gp_stack_top;

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

    gp_stack_bottom = &ld_pmm_stack_bottom;
    gp_stack_top_max = &ld_pmm_stack_top;
    gp_stack_top = gp_stack_top_max;

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

    if (gp_stack_top <= gp_stack_bottom)
    {
        // Cannot push the page - the stack is full.
        //
        return;
    }

    gp_stack_top--;
    *gp_stack_top = addr;
}

uint32_t
pmm_pop_page (void)
{
    if (gp_stack_top >= gp_stack_top_max)
    {
        // Cannot pop the page - the stack is empty.
        //
        printf("PMM: cannot pop page: stack is empty\n");
        panic("no free memory");
    }

    return (*gp_stack_top++);
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

        printf("PMM: size = %d, addr = 0x%X, end = 0x%X, type = %d\n",
               size, ((uint32_t) base_addr), ((uint32_t) (base_addr + length)),
               type);

        if ((base_addr > ((uint64_t) UINT_MAX))
            || (length > ((uint64_t) UINT_MAX))
            || ((base_addr + length) > ((uint64_t) UINT_MAX)))
        {
            printf("PMM: region lies outside of 4 GiB memory, ignoring it\n");
        }
        else if (MMAP_ENTRY_AVAILABLE == type)
        {
            add_region(((uint32_t) base_addr), ((uint32_t) length));
        }

        byte += (4 + size);
    }
}

static void
add_region (uint32_t start, uint32_t num_bytes)
{
    for (uint32_t page = start; page < (start + num_bytes); page += 4096)
    {
        pmm_push_page(page);
    }

    printf("PMM: added %u bytes starting at 0x%X\n", num_bytes, start);
}

static void
print_usage (void)
{
    uint32_t used_perc = (100 * ((uint32_t) (gp_stack_top_max - gp_stack_top))
                          / ((uint32_t) (gp_stack_top_max - gp_stack_bottom)));
    printf("PMM: stack is used for %u%%", used_perc);

    uint32_t num_bytes =
        (4096 * ((uint32_t) (gp_stack_top_max - gp_stack_top)));
    if (num_bytes <= (10 * 1024 * 1024))
    {
        printf(", holding %u KiB\n", (num_bytes / 1024));
    }
    else
    {
        printf(", holding %u MiB\n", (num_bytes / (1024 * 1024)));
    }
}
