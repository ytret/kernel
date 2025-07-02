#pragma once

#include <stdint.h>

#define VMM_USER_START 0x40000000

void vmm_init(void);

/// Returns the kernel page directory.
uint32_t const *vmm_kvas_dir(void);

/**
 * Performs a shallow clone of the kernel page directory.
 *
 * Present page directory entries that cover non-userspace memory are simply
 * copied. However, the page tables they are pointing at are not copied,
 * allowing sharing of the kernel page tables between tasks and processors.
 *
 * @returns
 * A heap-allocated page directory, where each entry points to the corresponding
 * kernel's page table.
 */
uint32_t *vmm_clone_kvas_dir(void);

void vmm_free_vas(uint32_t *p_dir);

void vmm_map_user_page(uint32_t *p_dir, uint32_t virt, uint32_t phys);
void vmm_map_kernel_page(uint32_t virt, uint32_t phys);
void vmm_unmap_kernel_page(uint32_t virt);

// Defined in vmm.s.
void vmm_load_dir(void const *p_dir);
