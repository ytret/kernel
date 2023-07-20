#include <alloc.h>
#include <panic.h>
#include <pmm.h>
#include <printf.h>
#include <vmm.h>

#include <stddef.h>
#include <stdint.h>

#define ADDR_TO_DIR_ENTRY_IDX(addr)     (((addr) >> 22) & 0x3FF)
#define ADDR_TO_TBL_ENTRY_IDX(addr)     (((addr) >> 12) & 0x3FF)

#define TABLE_USER      (1 << 2)
#define TABLE_RW        (1 << 1)
#define TABLE_PRESENT   (1 << 0)

#define PAGE_USER       (1 << 2)
#define PAGE_RW         (1 << 1)
#define PAGE_PRESENT    (1 << 0)

extern uint32_t ld_vmm_kernel_start;
extern uint32_t ld_vmm_kernel_end;

static uint32_t gp_kvas_dir[1024] __attribute__ ((aligned(4096)));
static uint32_t gpp_kvas_tables[2][1024] __attribute__ ((aligned(4096)));

static void map_page(uint32_t * p_dir, uint32_t virt, uint32_t phys,
                     uint32_t flags);
static void unmap_page(uint32_t * p_dir, uint32_t virt);
static void invlpg(uint32_t addr);

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

uint32_t const *
vmm_kvas_dir (void)
{
    return (gp_kvas_dir);
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
            p_dir[dir_idx] |= TABLE_USER; // temporary

            for (uint32_t tbl_idx = 0; tbl_idx < 1024; tbl_idx++)
            {
                // Map the same pages, with the same flags.
                //
                if (p_ktbl[tbl_idx] & PAGE_PRESENT)
                {
                    p_tbl[tbl_idx] = p_ktbl[tbl_idx];
                    p_tbl[tbl_idx] |= PAGE_USER; // temporary
                }
            }
        }
    }

    return (p_dir);
}

void
vmm_map_user_page (uint32_t * p_dir, uint32_t virt, uint32_t phys)
{
    map_page(p_dir, virt, phys, (PAGE_USER | PAGE_RW | PAGE_PRESENT));
}

void
vmm_map_kernel_page (uint32_t virt, uint32_t phys)
{
    map_page(gp_kvas_dir, virt, phys, (PAGE_RW | PAGE_PRESENT));
    invlpg(virt);
}

void
vmm_unmap_kernel_page (uint32_t virt)
{
    unmap_page(gp_kvas_dir, virt);
    invlpg(virt);
}

static void
map_page (uint32_t * p_dir, uint32_t virt, uint32_t phys, uint32_t flags)
{
    if (virt & 0xFFF)
    {
        printf("vmm: map_page: virt is not page-aligned\n");
        panic("invalid argument");
    }

    if (phys & 0xFFF)
    {
        printf("vmm: map_page: phys is not page-aligned\n");
        panic("invalid argument");
    }

    uint32_t dir_idx = ADDR_TO_DIR_ENTRY_IDX(virt);
    uint32_t tbl_idx = ADDR_TO_TBL_ENTRY_IDX(virt);

    uint32_t * p_tbl;
    if (p_dir[dir_idx] & TABLE_PRESENT)
    {
        printf("vmm: map_page: page table %u is present\n", dir_idx);

        if ((p_dir[dir_idx] & 0xFFF) != flags)
        {
            printf("vmm: map_page: page table for %P is present, but it"
                   " does not have the requested flags");
            panic("unexpected behavior");
        }

        p_tbl = ((uint32_t *) (p_dir[dir_idx] & ~0xFFF));
    }
    else
    {
        // Allocate a new page table.
        //
        p_tbl = alloc_aligned(4096, 4096);
        __builtin_memset(p_tbl, 0, 4096);

        printf("vmm: map_page: allocated a page table %u at %P\n",
               dir_idx, p_tbl);

        // Fill the dir entry.
        //
        p_dir[dir_idx] = (((uint32_t) p_tbl) | flags);
    }

    if (p_tbl[tbl_idx] != 0)
    {
        // The table entry is not empty.
        //
        printf("vmm: map_page: table entry %u for %P is not empty\n",
               tbl_idx, ((void *) virt));
        panic("unexpected behavior");
    }

    p_tbl[tbl_idx] = (phys | flags);
}

static void
unmap_page (uint32_t * p_dir, uint32_t virt)
{
    if (virt & 0xFFF)
    {
        printf("vmm: unmap_page: virt is not page-aligned\n");
        panic("invalid argument");
    }

    uint32_t dir_idx = ADDR_TO_DIR_ENTRY_IDX(virt);
    uint32_t tbl_idx = ADDR_TO_TBL_ENTRY_IDX(virt);

    if (!(p_dir[dir_idx] & TABLE_PRESENT))
    {
        printf("vmm: unmap_page: table %u for %P is not present\n",
               dir_idx, ((void *) virt));
        panic("unexpected behavior");
    }

    uint32_t * p_tbl = ((uint32_t *) (p_dir[dir_idx] & ~0xFFF));

    if (!(p_tbl[tbl_idx] & PAGE_PRESENT))
    {
        printf("vmm: unmap_page: page %u for %P is not present\n",
               tbl_idx, ((void *) virt));
        panic("unexpected behavior");
    }

    p_tbl[tbl_idx] = 0;
}

static void
invlpg (uint32_t addr)
{
    __asm__ volatile ("invlpg (%0)"
                      : /* no outputs */
                      : "r" (addr)
                      : "memory");
}
