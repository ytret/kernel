#pragma once

#include <stddef.h>
#include <stdint.h>

#include "list.h"
#include "types.h"

#define PMM_PAGE_SIZE 4096

typedef enum {
    PMM_REGION_AVAILABLE,
    PMM_REGION_RESERVED,

    PMM_REGION_KERNEL_RESERVED,
    PMM_REGION_KERNEL_AND_MODS,
    PMM_REGION_STATIC_HEAP,
} pmm_region_type_t;

typedef enum {
    PMM_ALLOC_NONE,  //!< This page is not allocated by anyone.
    PMM_ALLOC_SLAB,  //!< This page is owned by a slab allocator.
    PMM_ALLOC_LARGE, //!< This page is a part of a larger contiguous allocation.
} pmm_page_type_t;

typedef struct {
    list_node_t node;
    pmm_region_type_t type;
    uint64_t start;
    uint64_t end_incl;

    void *v_pools;
    size_t num_pools;

    size_t page_metadata_offset;
} pmm_region_t;

typedef struct {
    list_t entry_list;
} pmm_mmap_t;

typedef struct {
    pmm_page_type_t type;
    union {
        void *reserved; //!< Not used for free pages.
        void *slab;     //!< Slab allocator.
        void *large;    //!< Large allocation metadata.
    };
} pmm_page_t;

void pmm_init(const pmm_mmap_t *mmap);
const pmm_mmap_t *pmm_get_mmap(void);
void pmm_dump_mmap(void);

/**
 * Allocate contiguous physical memory pages.
 *
 * @returns Physical address to the allocated range (never `0`).
 *
 * @warning
 * This function panics if there is not enough physical memory.
 */
paddr_t pmm_alloc_pages(size_t num_pages);

/**
 * Allocate a contiguous aligned memory region.
 *
 * @param num_pages   Allocation size in pages.
 * @param align_pages Alignment in pages.
 *
 * @returns Physical address to the allocated range (never `0`).
 *
 * @warning
 * This function panics if the allocation could not be made.
 */
paddr_t pmm_alloc_aligned_pages(size_t num_pages, size_t align_pages);

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

pmm_page_t *pmm_paddr_to_page(paddr_t addr);
pmm_region_t *pmm_find_region_by_addr(paddr_t addr);

void pmm_push_page(uint32_t addr);
uint32_t pmm_pop_page(void);
