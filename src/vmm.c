#include <alloc.h>
#include <panic.h>
#include <pmm.h>
#include <printf.h>
#include <vmm.h>

#include <stddef.h>
#include <stdint.h>

#define ADDR_TO_DIR_ENTRY_IDX(addr)     (((addr) >> 22) & 0x3FF)
#define ADDR_TO_TBL_ENTRY_IDX(addr)     (((addr) >> 12) & 0x3FF)

#define TABLE_RW        (1 << 1)
#define TABLE_PRESENT   (1 << 0)

#define PAGE_RW         (1 << 1)
#define PAGE_PRESENT    (1 << 0)

extern uint32_t ld_vmm_kernel_start;
extern uint32_t ld_vmm_kernel_end;

static uint32_t gp_kvas_dir[1024] __attribute__ ((aligned(4096)));
static uint32_t gpp_kvas_tables[2][1024] __attribute__ ((aligned(4096)));

void
vmm_init (void)
{
    // Identity map the first 8 MiBs.
    //
    uint32_t map_end = 0x800000;

    // The kernel needs to be page-aligned for simplicity.
    //
    uint32_t kernel_start = ((uint32_t) &ld_vmm_kernel_start);
    uint32_t kernel_end   = ((uint32_t) &ld_vmm_kernel_end);
    if ((kernel_start & 0xFFF) || (kernel_end & 0xFFF))
    {
        printf("VMM: kernel boundaries are not page-aligned\n");
        panic("cannot init VMM");
    }

    for (uint32_t page = 0; page < map_end; page += 4096)
    {
        size_t dir_idx = ADDR_TO_DIR_ENTRY_IDX(page);
        size_t tbl_idx = ADDR_TO_TBL_ENTRY_IDX(page);

        if ((dir_idx != 0) && (dir_idx != 1))
        {
            printf("VMM: kernel pages do not fit into two page tables\n");
            panic("cannot init VMM");
        }

        gpp_kvas_tables[dir_idx][tbl_idx] = (page | PAGE_RW | PAGE_PRESENT);
    }

    gp_kvas_dir[0] =
        (((uint32_t) gpp_kvas_tables[0]) | TABLE_RW | TABLE_PRESENT);
    gp_kvas_dir[1] =
        (((uint32_t) gpp_kvas_tables[1]) | TABLE_RW | TABLE_PRESENT);

    // Enable paging.
    //
    vmm_load_dir(gp_kvas_dir);

    printf("VMM: memory range %P..%P is identity mapped\n", 0, map_end);
    printf("VMM: kernel start: %P, kernel end: %P\n", kernel_start, kernel_end);
}

uint32_t *
vmm_clone_kvas (void)
{
    uint32_t * p_dir = alloc_aligned(4096, 4096);
    __builtin_memset(p_dir, 0, 4096);

    for (uint32_t dir_idx = 0; dir_idx < 1024; dir_idx++)
    {
        if (gp_kvas_dir[dir_idx] & TABLE_PRESENT)
        {
            // KVAS table pointer.
            //
            uint32_t * p_ktbl = ((uint32_t *) (gp_kvas_dir[dir_idx] & ~0xFFF));

            // Allocate space for the new table.
            //
            uint32_t * p_tbl = alloc_aligned(4096, 4096);
            __builtin_memset(p_tbl, 0, 4096);

            // Fill the dir entry (flags are copied).
            //
            p_dir[dir_idx] =
                (((uint32_t) p_tbl) | (gp_kvas_dir[dir_idx] & 0xFFF));

            for (uint32_t tbl_idx = 0; tbl_idx < 1024; tbl_idx++)
            {
                // Map the same pages, with the same flags.
                //
                if (p_ktbl[tbl_idx] & PAGE_PRESENT)
                {
                    uint32_t page = pmm_pop_page();
                    p_tbl[tbl_idx] = (page | (p_ktbl[tbl_idx] & 0xFFF));
                }
            }
        }
    }

    return (p_dir);
}
