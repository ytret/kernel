#include <stddef.h>
#include <stdint.h>

#include "heap.h"
#include "kinttypes.h"
#include "kmutex.h"
#include "log.h"
#include "panic.h"
#include "pmm.h"
#include "smp.h"
#include "term.h"
#include "vmm.h"

#define VMM_ADDR_DIR_IDX(addr) (((addr) >> 22) & 0x3FF)
#define VMM_ADDR_TBL_IDX(addr) (((addr) >> 12) & 0x3FF)

#define VMM_TABLE_ADDR_MASK 0xFFFFF000U
#define VMM_TABLE_USER      (1 << 2)
#define VMM_TABLE_RW        (1 << 1)
#define VMM_TABLE_PRESENT   (1 << 0)

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

static void prv_vmm_lock_kvas(void);
static void prv_vmm_unlock_kvas(void);

static void prv_vmm_identity_map_ram(void);

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
        const uint32_t pgtbl_entry = pgtbl[pgtbl_idx];
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
