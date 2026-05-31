#pragma once

#include "kerr.h"
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
    paddr_t phys;
    vmm_prot_t prot;
    bool present;
} vmm_pginfo_t;

typedef struct vmm_vas vmm_vas_t;

extern int ld_kernel_vma;
extern int ld_kernel_lma;

void vmm2_init(void);

vmm_vas_t *vmm_new_vas(void);
void vmm2_free_vas(vmm_vas_t *vas);
vmm_vas_t *vmm_get_kvas(void);
void vmm_enter_vas(const vmm_vas_t *vas);

void vmm_map_region(vmm_vas_t *vas, vaddr_t virt, paddr_t phys,
                    size_t num_pages, vmm_prot_t prot);
void vmm_unmap_region(vmm_vas_t *vas, vaddr_t virt, size_t num_pages);

kerr_t vmm_set_prot(vmm_vas_t *vas, vaddr_t virt, vmm_prot_t new_prot);
bool vmm_query_page(const vmm_vas_t *vas, vaddr_t virt, vmm_pginfo_t *pginfo);

vaddr_t vmm_alloc_range(vmm_vas_t *vas, size_t num_pages, vmm_rgn_type_t type,
                        vmm_prot_t prot);
vaddr_t vmm_free_range(vmm_vas_t *vas, vaddr_t start, size_t num_pages);
vaddr_t vmm_alloc_and_map(vmm_vas_t *vas, size_t num_pages, vmm_rgn_type_t type,
                          vmm_prot_t prot);

void vmm_invlpg(vaddr_t vaddr);
