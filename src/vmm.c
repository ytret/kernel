#include <panic.h>
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
    // Identity map the kernel.
    //
    uint32_t kernel_start = ((uint32_t) &ld_vmm_kernel_start);
    uint32_t kernel_end   = ((uint32_t) &ld_vmm_kernel_end);

    if ((kernel_start & 0xFFF) || (kernel_end & 0xFFF))
    {
        printf("VMM: kernel boundaries are not page-aligned\n");
        panic("cannot init VMM");
    }

    for (uint32_t page = 0; page < kernel_end; page += 4096)
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

    printf("VMM: memory range %P..%P is identity mapped\n", 0, kernel_end);
}
