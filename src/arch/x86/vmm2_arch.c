#include "vmm2_arch.h"

struct vmm_vas_arch {};

// linker.ld
extern uint32_t ld_boot_end;

void vmm_arch_get_boot_rgn(vaddr_t *out_start, vaddr_t *out_end_incl) {
    if (out_start) { *out_start = 0; }
    if (out_end_incl) { *out_end_incl = (vaddr_t)&ld_boot_end - 1; }
}

void vmm_arch_map(vmm_vas_arch_t *vas, vaddr_t virt, paddr_t phys,
                  vmm_prot_t prot);
void vmm_arch_unmap(vmm_vas_arch_t *vas, vaddr_t virt);
void vmm_arch_query(const vmm_vas_arch_t *vas, vaddr_t virt,
                    vmm_pginfo_t *pginfo);

void vmm_arch_tlb_flush(vaddr_t virt);
void vmm_arch_tlb_flush_all(void);
