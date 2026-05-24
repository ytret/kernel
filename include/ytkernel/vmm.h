#pragma once

#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define VMM_USER_START 0x40000000
#define VMM_USER_END   0xE0000000

#define VMM_KERNEL_VMA ((uintptr_t)&ld_kernel_vma)
#define VMM_KERNEL_LMA ((uintptr_t)&ld_kernel_lma)

extern int ld_kernel_vma;
extern int ld_kernel_lma;

void vmm_init(void);

/// Returns the kernel page directory.
void *vmm_kvas_dir(void);

void vmm_free_vas(uint32_t *p_dir);

void vmm_map_user_page(uint32_t *p_dir, uint32_t virt, uint32_t phys);
void vmm_map_kernel_page(uint32_t virt, uint32_t phys);
void vmm_unmap_kernel_page(uint32_t virt);
void vmm_kmap_region_a(void *(*alloc)(size_t size), vaddr_t start,
                       vaddr_t size);
void vmm_kmap_region(vaddr_t start, vaddr_t size);
bool vmm_kmap_region_in(void *pgtbl, vaddr_t start, vaddr_t size);

/**
 * Identity maps physical pages containing a buffer.
 *
 * @param buf  Buffer that needs to be identity mapped.
 * @param size Size of @a buf in bytes.
 */
void vmm_kmap_buf(const void *buf, size_t size);

void vmm_invlpg(uint32_t virt);

bool vmm_is_paging_enabled(void);
bool vmm_is_addr_mapped(uint32_t virt);

// Defined in vmm.s.
void vmm_load_dir(void const *p_dir);
