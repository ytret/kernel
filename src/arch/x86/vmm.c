#include <stddef.h>
#include <stdint.h>

#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "heap.h"
#include "kinttypes.h"
#include "kmutex.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"
#include "smp.h"
#include "term.h"
#include "vmm.h"

#define VMM_ADDR_DIR_IDX(addr) (((addr) >> 22) & 0x3FF)
#define VMM_ADDR_TBL_IDX(addr) (((addr) >> 12) & 0x3FF)
#define VMM_ADDR_PAGE_MASK     ~0xFFF

#define VMM_TABLE_ADDR_MASK 0xFFFFF000U
#define VMM_TABLE_USER      (1 << 2)
#define VMM_TABLE_RW        (1 << 1)
#define VMM_TABLE_PRESENT   (1 << 0)
#define VMM_TABLE_SIZE      4096U

#define VMM_PAGE_USER    (1 << 2)
#define VMM_PAGE_RW      (1 << 1)
#define VMM_PAGE_PRESENT (1 << 0)

#define VMM_CR0_PGEN (1 << 31)

/**
 * Table entry flags that are checked when comparing two page tables.
 * If the two page table entries have different values when this mask is
 * applied, they are considered different.
 */
#define VMM_TBL_EQ_FLAGS (VMM_TABLE_USER | VMM_TABLE_RW | VMM_TABLE_PRESENT)

static uint32_t *gp_kvas_dir;
static task_mutex_t g_kvas_lock;

extern uint32_t *boot_pgtbls;
extern uint32_t boot_pgtbls_end;

static void prv_vmm_lock_kvas(void);
static void prv_vmm_unlock_kvas(void);

static void prv_vmm_identity_map_ram(void);
static void prv_vmm_unmap_zero_page(void);

static void map_page(uint32_t *p_dir, uint32_t virt, uint32_t phys,
                     uint32_t flags);
static void unmap_page(uint32_t *p_dir, uint32_t virt);

static uint32_t prv_vmm_read_cr0(void);
static uint32_t prv_vmm_read_cr3(void);

void vmm_init(void) {
    gp_kvas_dir = heap_alloc_aligned(4096, 4096);
    __builtin_memset(gp_kvas_dir, 0, 4096);
    LOG_DEBUG("kernel page dir is at %p", gp_kvas_dir);

    prv_vmm_identity_map_ram();
    prv_vmm_unmap_zero_page();

    term_map_iomem();

    vmm_load_dir(gp_kvas_dir);
}

uint32_t const *vmm_kvas_dir(void) {
    return gp_kvas_dir;
}

void vmm_free_vas(uint32_t *p_dir) {
    (void)p_dir;
}

void vmm_map_user_page(uint32_t *p_dir, uint32_t virt, uint32_t phys) {
    map_page(p_dir, virt, phys,
             (VMM_PAGE_USER | VMM_PAGE_RW | VMM_PAGE_PRESENT));
}

void vmm_map_kernel_page(uint32_t virt, uint32_t phys) {
    prv_vmm_lock_kvas();

    map_page(gp_kvas_dir, virt, phys, (VMM_PAGE_RW | VMM_PAGE_PRESENT));
    vmm_invlpg(virt);
    if (smp_is_active()) { smp_send_tlb_shootdown(virt); }

    prv_vmm_unlock_kvas();
}

void vmm_unmap_kernel_page(uint32_t virt) {
    prv_vmm_lock_kvas();

    unmap_page(gp_kvas_dir, virt);
    vmm_invlpg(virt);
    if (smp_is_active()) { smp_send_tlb_shootdown(virt); }

    prv_vmm_unlock_kvas();
}

void vmm_kmap_region(void *(*alloc)(size_t size), vaddr_t start, vaddr_t size) {
    vaddr_t end = start + size;
    LOG_FLOW("identity map region 0x%08" PRIx32 "..0x%08" PRIx32
             " (size 0x%08" PRIx32 ")",
             start, end, size);

    if (start & ~VMM_ADDR_PAGE_MASK) {
        PANIC("invalid argument 'start' value 0x%08" PRIx32
              ", it must be page-aligned (%u)",
              start, PMM_PAGE_SIZE);
    }
    if (size == 0) { return; }
    if (size % PMM_PAGE_SIZE != 0) {
        PANIC("invalid argument 'size' value 0x%08" PRIx32
              ", it must be page-aligned (%u)",
              size, PMM_PAGE_SIZE);
    }

    if (!vmm_is_paging_enabled()) { PANIC("paging is not enabled"); }

    uint32_t *pgdir;

    if (gp_kvas_dir) {
        pgdir = gp_kvas_dir;
    } else {
        uint32_t cr3 = prv_vmm_read_cr3();
        pgdir = (uint32_t *)cr3; // FIXME: PHYS_TO_VIRT?
    }

    prv_vmm_lock_kvas();

    for (vaddr_t addr = start; addr < end; addr += PMM_PAGE_SIZE) {
        const uint32_t pgdir_idx = VMM_ADDR_DIR_IDX(addr);
        const uint32_t pgtbl_idx = VMM_ADDR_TBL_IDX(addr);

        uint32_t pgdir_entry = pgdir[pgdir_idx];
        uint32_t *pgtbl;
        if (pgdir_entry & VMM_TABLE_PRESENT) {
            uint32_t need_flags = VMM_TABLE_RW | VMM_TABLE_PRESENT;
            if ((pgdir_entry & VMM_TBL_EQ_FLAGS) != need_flags) {
                PANIC("cannot map page 0x%08" PRIx32 ", page dir entry %" PRIu32
                      " has incompatible flags (0x%08" PRIx32 ")",
                      addr, pgdir_idx, pgdir_entry);
            }
            pgtbl = (uint32_t *)(pgdir_entry & VMM_TABLE_ADDR_MASK);
        } else {
            pgtbl = alloc(VMM_TABLE_SIZE);
            if ((uintptr_t)pgtbl & (PMM_PAGE_SIZE - 1)) {
                PANIC("alloc returned an unaligned address 0x%08" PRIxPTR,
                      (uintptr_t)pgtbl);
            }
            LOG_FLOW("alloc page table 0x%08" PRIxPTR
                     " for dir idx %u page 0x%08" PRIx32 "",
                     (uintptr_t)pgtbl, pgdir_idx, addr);
            kmemset(pgtbl, 0, VMM_TABLE_SIZE);

            const uint32_t flags = VMM_TABLE_RW | VMM_TABLE_PRESENT;
            if ((uintptr_t)pgtbl >= VMM_KERNEL_VMA) {
                pgdir_entry = (uint32_t)VIRT_TO_PHYS(pgtbl) | flags;
            } else {
                pgdir_entry = (uint32_t)pgtbl | flags;
            }

            pgdir[pgdir_idx] = pgdir_entry;
        }

        if (vmm_is_paging_enabled() && !vmm_is_addr_mapped((uint32_t)pgtbl)) {
            vaddr_t himem_pgtbl = PHYS_TO_VIRT((uintptr_t)pgtbl);
            if (!vmm_is_addr_mapped(himem_pgtbl)) {
                LOG_FLOW("page table covering 0x%08" PRIx32
                         " at physical 0x%08" PRIxPTR ", virtual 0x%08" PRIx32
                         " is unmapped",
                         addr, (uintptr_t)pgtbl, himem_pgtbl);
            }
            pgtbl = (uint32_t *)himem_pgtbl;
        }

        uint32_t pgtbl_entry = pgtbl[pgtbl_idx];
        if (pgtbl_entry & VMM_PAGE_PRESENT) {
            LOG_ERROR("found present pgtbl entry idx %" PRIu32
                      " page 0x%08" PRIx32 ": 0x%08" PRIx32,
                      pgtbl_idx, addr, pgtbl_entry);
            PANIC("cannot map page 0x%08" PRIx32 ", page tbl 0x%08" PRIxPTR
                  " entry %" PRIu32 " is already present",
                  addr, (uintptr_t)pgtbl, pgtbl_idx);
        }

        pgtbl_entry = addr | VMM_PAGE_RW | VMM_PAGE_PRESENT;
        pgtbl[pgtbl_idx] = pgtbl_entry;
        LOG_FLOW("identity mapped page 0x%08" PRIx32 " (pgdir 0x%08" PRIxPTR
                 " idx %u, pgtbl 0x%08" PRIxPTR " idx %u)",
                 addr, (uintptr_t)pgdir, pgdir_idx, (uintptr_t)pgtbl,
                 pgtbl_idx);

        vmm_invlpg(addr);
    }

    prv_vmm_unlock_kvas();
}

void vmm_invlpg(uint32_t virt) {
    __asm__ volatile("invlpg (%0)"
                     : /* no outputs */
                     : "r"(virt)
                     : "memory");
}

bool vmm_is_paging_enabled(void) {
    const uint32_t cr0 = prv_vmm_read_cr0();
    return cr0 & VMM_CR0_PGEN;
}

bool vmm_is_addr_mapped(uint32_t virt) {
    const uint32_t pgdir_idx = VMM_ADDR_DIR_IDX(virt);
    const uint32_t pgtbl_idx = VMM_ADDR_TBL_IDX(virt);

    if (!vmm_is_paging_enabled()) {
        LOG_ERROR("paging is not enabled, address 0x%08" PRIx32
                  " is considered unmapped",
                  virt);
        return false;
    }

    const uint32_t *const pgdir = (const uint32_t *)prv_vmm_read_cr3();
    if (!pgdir) { return false; }

    const uint32_t pgdir_entry = pgdir[pgdir_idx];
    if (pgdir_entry & VMM_TABLE_PRESENT) {
        const uint32_t *const pgtbl =
            (const uint32_t *)(pgdir_entry & VMM_TABLE_ADDR_MASK);
        const uint32_t pgtbl_addr = (uint32_t)pgtbl;

        uint32_t pgtbl_entry;
        if ((vaddr_t)boot_pgtbls <= (vaddr_t)pgtbl_addr &&
            (vaddr_t)pgtbl_addr < (vaddr_t)&boot_pgtbls_end) {
            pgtbl_entry = pgtbl[pgtbl_idx];
        } else if (vmm_is_addr_mapped((vaddr_t)pgtbl_addr)) {
            pgtbl_entry = pgtbl[pgtbl_idx];
        } else if (vmm_is_addr_mapped((vaddr_t)PHYS_TO_VIRT(pgtbl_addr))) {
            pgtbl_entry =
                ((uint32_t *)(vaddr_t)PHYS_TO_VIRT(pgtbl_addr))[pgtbl_idx];
        } else {
            return false;
        }

        return pgtbl_entry & VMM_PAGE_PRESENT;
    }

    return false;
}

static void prv_vmm_identity_map_ram(void) {
    const pmm_mmap_t *const physmem_map = pmm_get_mmap();

    for (list_node_t *node = physmem_map->entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        if (region->type != PMM_REGION_AVAILABLE &&
            region->type != PMM_REGION_KERNEL_RESERVED &&
            region->type != PMM_REGION_KERNEL_AND_MODS &&
            region->type != PMM_REGION_STATIC_HEAP) {
            continue;
        }

        if (region->end_incl >= UINT32_MAX) {
            LOG_DEBUG("skip region 0x%016llx .. 0x%016llx", region->start,
                      region->end_incl);
            continue;
        }

        const uint32_t map_start = region->start & ~(PMM_PAGE_SIZE - 1);
        const uint32_t map_end =
            ((region->end_incl + 1) + (PMM_PAGE_SIZE - 1)) &
            ~(PMM_PAGE_SIZE - 1);

        LOG_DEBUG("identity map range %p .. %p", (void *)map_start,
                  (void *)map_end);
        for (uint32_t page = map_start; page < map_end; page += 4096) {
            vmm_map_kernel_page(page, page);
        }
    }
}

static void prv_vmm_unmap_zero_page(void) {
    uint32_t dir_entry0 = gp_kvas_dir[0];
    uint32_t *const tbl = (uint32_t *)(dir_entry0 & VMM_TABLE_ADDR_MASK);
    if (!tbl) { return; }
    tbl[0] = 0;
}

static void prv_vmm_lock_kvas(void) {
    if (smp_is_active()) { mutex_acquire(&g_kvas_lock); }
}

static void prv_vmm_unlock_kvas(void) {
    if (smp_is_active()) { mutex_release(&g_kvas_lock); }
}

static void map_page(uint32_t *p_dir, uint32_t virt, uint32_t phys,
                     uint32_t flags) {
    if (!p_dir) { PANIC("invalid argument 'p_dir' value NULL"); }
    if (virt & 0xFFF) {
        PANIC("invalid argument 'virt' value 0x%08" PRIx32
              " - not page-aligned",
              virt);
    }
    if (phys & 0xFFF) {
        PANIC("invalid argument 'phys' value 0x%08" PRIx32
              " - not page-aligned",
              phys);
    }

    uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    uint32_t *p_tbl;
    if (p_dir[dir_idx] & VMM_TABLE_PRESENT) {
        if ((p_dir[dir_idx] & VMM_TBL_EQ_FLAGS) != flags) {
            PANIC("page table for %p is present, but its checked flags "
                  "0x%03" PRIx32 " are different from 0x%03" PRIx32,
                  (void *)virt, (p_dir[dir_idx] & VMM_TBL_EQ_FLAGS), flags);
        }

        p_tbl = ((uint32_t *)(p_dir[dir_idx] & ~0xFFF));
    } else {
        // Allocate a new page table.
        p_tbl = heap_alloc_aligned(4096, 4096);
        __builtin_memset(p_tbl, 0, 4096);

        // Fill the dir entry.
        p_dir[dir_idx] = (((uint32_t)p_tbl) | flags);
    }

    if (p_tbl[tbl_idx] != 0) {
        // The table entry is not empty.
        PANIC("table entry %zu for %p is not empty", tbl_idx, ((void *)virt));
    }

    p_tbl[tbl_idx] = (phys | flags);
}

static void unmap_page(uint32_t *p_dir, uint32_t virt) {
    if (virt & 0xFFF) {
        PANIC("invalid argument 'virt' value 0x%08" PRIx32
              " - not page-aligned",
              virt);
    }

    uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    if (!(p_dir[dir_idx] & VMM_TABLE_PRESENT)) {
        PANIC("table %zu for %p is not present", dir_idx, ((void *)virt));
    }

    uint32_t *p_tbl = ((uint32_t *)(p_dir[dir_idx] & ~0xFFF));

    if (!(p_tbl[tbl_idx] & VMM_PAGE_PRESENT)) {
        PANIC("page %zu for %p is not present", tbl_idx, ((void *)virt));
    }

    p_tbl[tbl_idx] = 0;
}

static uint32_t prv_vmm_read_cr0(void) {
    uint32_t reg_val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(reg_val));
    return reg_val;
}

static uint32_t prv_vmm_read_cr3(void) {
    uint32_t reg_val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(reg_val));
    return reg_val;
}
