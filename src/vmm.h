#pragma once

#include <mbi.h>

#include <stdint.h>

#define VMM_HEAP_START  0x400000
#define VMM_HEAP_SIZE   0x400000

#define VMM_USER_START  0x40000000

void vmm_init(mbi_t const * p_mbi);

uint32_t const * vmm_kvas_dir(void);
uint32_t       * vmm_clone_kvas(void);

void vmm_map_user_page(uint32_t * p_dir, uint32_t virt, uint32_t phys);
void vmm_map_kernel_page(uint32_t virt, uint32_t phys);
void vmm_unmap_kernel_page(uint32_t virt);

// Defined in vmm.s.
//
void vmm_load_dir(void const * p_dir);
