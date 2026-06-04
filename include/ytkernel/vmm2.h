#pragma once

#include "types.h"

#define VMM_USER_START 0x40000000
#define VMM_USER_END   0xE0000000

#define VMM_KERNEL_VMA ((uintptr_t)&ld_kernel_vma)
#define VMM_KERNEL_LMA ((uintptr_t)&ld_kernel_lma)

// FIXME
#define VMM_PAGE_SIZE 4096

typedef enum {
    VMM_REGION_UNUSED,
    VMM_REGION_BOOT,
    VMM_REGION_KERNEL,
} vmm_rgn_type_t;

typedef enum {
    // VMM_PROT_KERNEL = 1 << 0,
    VMM_PROT_USER = 1 << 1,
    VMM_PROT_READ = 1 << 2,
    VMM_PROT_WRITE = 1 << 3,
    VMM_PROT_EXEC = 1 << 4,
    // TODO: cache?
} vmm_prot_t;

typedef struct {
    bool present;
    paddr_t phys;
    vmm_prot_t prot;
} vmm_pginfo_t;

typedef struct vmm_vas vmm_vas_t;

extern int ld_kernel_vma;
extern int ld_kernel_lma;

void vmm2_init(void);

vmm_vas_t *vmm_new_vas(void);
void vmm2_free_vas(vmm_vas_t *vas);
vmm_vas_t *vmm_get_kvas(void);
void vmm_enter_vas(const vmm_vas_t *vas);

/**
 * Map the virtual memory range to the physical memory range.
 */
bool vmm_map_range(vmm_vas_t *vas, vaddr_t virt, paddr_t phys, size_t num_pages,
                   vmm_rgn_type_t type, vmm_prot_t prot);
/**
 * Find a virtual memory range of given size and map it anywhere.
 */
bool vmm_alloc(vmm_vas_t *vas, size_t num_pages, vmm_rgn_type_t type,
               vmm_prot_t prot);
/**
 * Find a virtual memory range and map it to the given physical memory.
 */
bool vmm_alloc_and_map(vmm_vas_t *vas, vaddr_t phys, size_t num_pages,
                       vmm_rgn_type_t type, vmm_prot_t prot);
vaddr_t vmm_free_range(vmm_vas_t *vas, vaddr_t start, size_t num_pages);

bool vmm_query_page(const vmm_vas_t *vas, vaddr_t virt, vmm_pginfo_t *pginfo);
void vmm_invlpg(vaddr_t vaddr);
