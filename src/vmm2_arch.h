#pragma once

#include "kerr.h"
#include "vmm2.h"

#define VMM_ADDR_PAGE_MASK  0xFFFFF000U
#define VMM_ADDR_PGOFF_MASK 0x00000FFFU
#define VMM_ADDR_PAGE(x)    ((x) & VMM_ADDR_PAGE_MASK)
#define VMM_ADDR_PGOFF(x)   ((x) & VMM_ADDR_PGOFF_MASK)

typedef struct vmm_vas_arch vmm_vas_arch_t;

void vmm_arch_get_boot_rgn(vaddr_t *out_start, vaddr_t *out_end_incl);

kerr_t vmm_arch_map(vmm_vas_arch_t *vas, vaddr_t virt, paddr_t phys,
                    vmm_prot_t prot);
kerr_t vmm_arch_unmap(vmm_vas_arch_t *vas, vaddr_t virt);
void vmm_arch_query(const vmm_vas_arch_t *vas, vaddr_t virt,
                    vmm_pginfo_t *pginfo);

void vmm_arch_tlb_flush(vaddr_t virt);
void vmm_arch_tlb_flush_all(void);
