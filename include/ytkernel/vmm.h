#pragma once

#include <stdint.h>

#define VMM_USER_START 0x40000000
#define VMM_USER_END   0xE0000000

void vmm_init(void);

/// Returns the kernel page directory.
uint32_t const *vmm_kvas_dir(void);

void vmm_free_vas(uint32_t *p_dir);

void vmm_map_user_page(uint32_t *p_dir, uint32_t virt, uint32_t phys);
void vmm_map_kernel_page(uint32_t virt, uint32_t phys);
void vmm_unmap_kernel_page(uint32_t virt);
void vmm_invlpg(uint32_t virt);

bool vmm_is_paging_enabled(void);
bool vmm_is_addr_mapped(uint32_t virt);

// Defined in vmm.s.
void vmm_load_dir(void const *p_dir);
