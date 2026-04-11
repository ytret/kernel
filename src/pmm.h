#pragma once

#include <stddef.h>
#include <stdint.h>

#include "list.h"
#include "types.h"

typedef enum {
    PMM_REGION_AVAILABLE,
    PMM_REGION_RESERVED,

    PMM_REGION_KERNEL_RESERVED,
    PMM_REGION_KERNEL_AND_MODS,
    PMM_REGION_STATIC_HEAP,
} pmm_region_type_t;

typedef struct {
    list_node_t node;
    pmm_region_type_t type;
    uint64_t start;
    uint64_t end_incl;

    void *v_pools;
    size_t num_pools;
} pmm_region_t;

typedef struct {
    list_t entry_list;
} pmm_mmap_t;

void pmm_init(const pmm_mmap_t *mmap);
void pmm_print_mmap(void);

/**
 * Allocate contiguous physical memory pages.
 *
 * @returns Address to the allocated range (never `NULL`).
 *
 * @warning
 * This function panics if there is not enough physical memory.
 */
paddr_t pmm_alloc_pages(size_t num_pages);

/**
 * Free previously allocated physical memory.
 *
 * @param addr      Address returned by #pmm_alloc_pages() or `NULL`.
 * @param num_pages Number of pages passed to #pmm_alloc_pages().
 *
 * @warning
 * If @a addr and/or @a num_pages are not what was returned from or passed to
 * #pmm_alloc_pages(), this function may either panic or corrupt memory.
 */
void pmm_free_pages(paddr_t addr, size_t num_pages);

void pmm_push_page(uint32_t addr);
uint32_t pmm_pop_page(void);
