#include "assert.h"
#include "heap.h"
#include "kerr.h"
#include "kinttypes.h"
#include "memfun.h"
#include "pmm.h"
#include "vmm2_arch.h"

#define VMM_ADDR_DIR_IDX(addr) (((addr) >> 22) & 0x3FF)
#define VMM_ADDR_TBL_IDX(addr) (((addr) >> 12) & 0x3FF)

#define VMM_PGDIR_SIZE  4096U
#define VMM_PGDIR_ALIGN 4096U

#define VMM_PGTBL_SIZE  4096U
#define VMM_PGTBL_ALIGN 4096U

#define VMM_PDE_ADDR_MASK  0xFFFFF000U
#define VMM_PDE_FLAGS_MASK 0x00000FFFU
#define VMM_PDE_USER       (1 << 2)
#define VMM_PDE_RW         (1 << 1)
#define VMM_PDE_PRESENT    (1 << 0)

#define VMM_PTE_ADDR_MASK  0xFFFFF000U
#define VMM_PTE_FLAGS_MASK 0x00000FFFU
#define VMM_PTE_USER       (1 << 2)
#define VMM_PTE_RW         (1 << 1)
#define VMM_PTE_PRESENT    (1 << 0)
#define VMM_PTE_PROT_MASK  (VMM_PTE_USER | VMM_PTE_RW | VMM_PTE_PRESENT)

#define VMM_CR0_PGEN (1 << 31)

struct vmm_vas_arch {
    uint32_t *pgdir;
    uint32_t pgdir_phys;
};

// linker.ld
extern uint32_t ld_boot_end;

static bool prv_vmm_check_pde_flags(uint32_t flags, vmm_prot_t page_prot);
static bool prv_vmm_check_pte_flags(uint32_t flags, vmm_prot_t page_prot);

static uint32_t prv_vmm_get_pde_flags(vmm_prot_t prot);
static uint32_t prv_vmm_get_pte_flags(vmm_prot_t prot);
static vmm_prot_t prv_vmm_get_prot(uint32_t pte);

extern void vmm_load_dir(void const *p_dir);

void vmm_arch_init_vas(vmm_vas_arch_t *vas) {
    ASSERT(vas != NULL);
    kmemset(vas, 0, sizeof(vmm_vas_arch_t));

    void *const pgdir = pmm_alloc_pgtable();
    vas->pgdir = pgdir;
    vas->pgdir_phys = (uint32_t)pgdir; // FIXME: paddr_t when it becomes 32-bit
}

vmm_vas_arch_t *vmm_arch_new_vas(void) {
    vmm_vas_arch_t *vas =
        heap_alloc_aligned(sizeof(vmm_vas_arch_t), _Alignof(vmm_vas_arch_t));
    vmm_arch_init_vas(vas);
    return vas;
}

void vmm_arch_free_vas(vmm_vas_arch_t *vas) {
    DEBUG_ASSERT(vas != NULL);
    DEBUG_ASSERT(vas->pgdir != NULL);
    heap_free(vas->pgdir);
    heap_free(vas);
}

void vmm_arch_enter_vas(vmm_vas_arch_t *vas) {
    DEBUG_ASSERT(vas != NULL);
    DEBUG_ASSERT(vas->pgdir != NULL);

    vmm_load_dir((void *)vas->pgdir_phys);
}

void vmm_arch_get_boot_rgn(vaddr_t *out_start, vaddr_t *out_end_incl) {
    if (out_start) { *out_start = 0; }
    if (out_end_incl) { *out_end_incl = (vaddr_t)&ld_boot_end - 1; }
}

kerr_t vmm_arch_map(vmm_vas_arch_t *vas_arch, vaddr_t virt, paddr_t phys,
                    vmm_prot_t prot) {
    DEBUG_ASSERT(vas_arch != NULL);
    DEBUG_ASSERT(vas_arch->pgdir != NULL);
    ASSERT(VMM_ADDR_PGOFF(virt) == 0);
    ASSERT(VMM_ADDR_PGOFF(phys) == 0);

    uint32_t *const pgdir = vas_arch->pgdir;

    const uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    const uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    uint32_t *pgtbl;
    if (pgdir[dir_idx] & VMM_PDE_PRESENT) {
        const uint32_t used_addr = pgdir[dir_idx] & VMM_PDE_ADDR_MASK;
        const uint32_t used_flags = pgdir[dir_idx] & VMM_PDE_FLAGS_MASK;

        if (prv_vmm_check_pde_flags(used_flags, prot)) {
            return KERR_INCOMP_MAP;
        }

        // NOTE: it is assumed that the page table is identity mapped.
        pgtbl = (uint32_t *)used_addr;
    } else {
        pgtbl = pmm_alloc_pgtable();
        kmemset(pgtbl, 0, VMM_PGTBL_SIZE);

        const uint32_t pde_flags = prv_vmm_get_pde_flags(prot);
        pgdir[dir_idx] = (uint32_t)pgtbl | pde_flags;
    }

    if (pgtbl[tbl_idx] & VMM_PTE_PRESENT) {
        const uint32_t used_addr = pgtbl[tbl_idx] & VMM_PTE_ADDR_MASK;
        const uint32_t used_flags = pgtbl[tbl_idx] & VMM_PTE_FLAGS_MASK;

        if (used_addr != phys) { return KERR_MAPPED; }
        if (prv_vmm_check_pte_flags(used_flags, prot)) { return KERR_MAPPED; }
    } else {
        const uint32_t flags = prv_vmm_get_pte_flags(prot);

        pgtbl[tbl_idx] = phys | flags;
    }

    return KERR_NONE;
}

kerr_t vmm_arch_unmap(vmm_vas_arch_t *vas, vaddr_t virt) {
    DEBUG_ASSERT(vas != NULL);
    ASSERT(vas->pgdir != NULL);
    uint32_t *const pgdir = vas->pgdir;

    if (virt & 0xFFF) {
        PANIC("invalid argument 'virt' value 0x%08" PRIx32
              " - not page-aligned",
              virt);
    }

    const uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    const uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    if (!(pgdir[dir_idx] & VMM_PDE_PRESENT)) { return KERR_NOT_MAPPED; }

    // NOTE: it is assumed that the PDE address is identity mapped.
    uint32_t *const pgtbl = (uint32_t *)(pgdir[dir_idx] & VMM_PDE_ADDR_MASK);
    if (!(pgtbl[tbl_idx] & VMM_PTE_PRESENT)) { return KERR_NOT_MAPPED; }

    pgtbl[tbl_idx] = 0;
    return KERR_NONE;
}

void vmm_arch_query(const vmm_vas_arch_t *vas, vaddr_t virt,
                    vmm_pginfo_t *out_pginfo) {
    DEBUG_ASSERT(vas != NULL);
    DEBUG_ASSERT(out_pginfo != NULL);
    ASSERT(vas->pgdir != NULL);

    const uint32_t *const pgdir = vas->pgdir;
    out_pginfo->present = false;
    out_pginfo->phys = 0;
    out_pginfo->prot = 0;

    if (virt & 0xFFF) {
        PANIC("invalid argument 'virt' value 0x%08" PRIx32
              " - not page-aligned",
              virt);
    }

    const uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    const uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    if (pgdir[dir_idx] & VMM_PDE_PRESENT) {
        // NOTE: it is assumed that the PDE address is identity mapped.
        uint32_t *const pgtbl =
            (uint32_t *)(pgdir[dir_idx] & VMM_PDE_ADDR_MASK);
        const uint32_t pte = pgtbl[tbl_idx];
        if (pte & VMM_PTE_PRESENT) { out_pginfo->present = true; }
        out_pginfo->phys = pte & VMM_PTE_ADDR_MASK;
        out_pginfo->prot = prv_vmm_get_prot(pte);
    }
}

void vmm_arch_tlb_flush(vaddr_t virt) {
    __asm__ volatile("invlpg (%0)"
                     : /* no outputs */
                     : "r"(virt)
                     : "memory");
}

void vmm_arch_tlb_flush_all(void) {
    PANIC("TODO %s", __func__);
}

/**
 * Checks whether page table flags permit to map a page with the given flags.
 */
static bool prv_vmm_check_pde_flags(uint32_t flags, vmm_prot_t page_prot) {
    const bool tbl_is_user = (flags & VMM_PDE_USER) != 0;
    const bool tbl_is_rw = (flags & VMM_PDE_RW) != 0;

    const bool page_is_user = (page_prot & VMM_PROT_USER) != 0;
    const bool page_is_rw = (page_prot & VMM_PROT_WRITE) != 0;

    if (tbl_is_user != page_is_user) { return false; }
    if (tbl_is_rw != page_is_rw) { return false; }

    return true;
}

/**
 * Checks whether @a flags represent permissions @a page_prot.
 */
static bool prv_vmm_check_pte_flags(uint32_t flags, vmm_prot_t page_prot) {
    const uint32_t prot_flags = prv_vmm_get_pte_flags(page_prot);
    return prot_flags == (flags & VMM_PTE_PROT_MASK);
}

/**
 * Returns lax flags for a table covering pages with @a prot permissions.
 */
static uint32_t prv_vmm_get_pde_flags(vmm_prot_t prot) {
    uint32_t flags = VMM_PDE_RW | VMM_PDE_PRESENT;

    if (prot & VMM_PROT_USER) { flags |= VMM_PDE_USER; }

    return flags;
}

/**
 * Returns page flags best matching @a prot.
 */
static uint32_t prv_vmm_get_pte_flags(vmm_prot_t prot) {
    uint32_t flags = VMM_PTE_PRESENT;

    if (prot & VMM_PROT_USER) { flags |= VMM_PTE_USER; }
    if (!(prot & VMM_PROT_READ)) {
        // FIXME: return an error?
        PANIC("x86 does not support no VMM_PROT_READ");
    }
    if (prot & VMM_PROT_WRITE) { flags |= VMM_PTE_RW; }
    // Ignore VMM_PROT_EXEC. Everything is executable on 32-bit x86 without PAE.

    return flags;
}

static vmm_prot_t prv_vmm_get_prot(uint32_t pte) {
    // Everything is readable and executable on 32-bit x86 without PAE.
    vmm_prot_t prot = VMM_PROT_READ | VMM_PROT_EXEC;

    if (pte & VMM_PTE_USER) { prot |= VMM_PROT_USER; }
    if (pte & VMM_PTE_RW) { prot |= VMM_PROT_WRITE; }

    return prot;
}
