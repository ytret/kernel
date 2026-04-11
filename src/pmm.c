#include <ytalloc/ytalloc.h>

#include "heap.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"

#define PMM_RESERVE_LOWER_BYTES (4U * 1024U * 1024U)

#define PMM_BUDDY_MAX_ORDERS     20
#define PMM_LOG2_MIN_POOL_SIZE   22 // log2 of 4 MiB
#define PMM_MIN_POOL_SIZE        (1 << PMM_LOG2_MIN_POOL_SIZE)
#define PMM_MAX_POOLS_PER_REGION 16

typedef struct {
    uint32_t start_addr;
    size_t size;
    alloc_buddy_t *heap;
} pmm_buddy_pool_t;

typedef struct {
    size_t available_ram_size;
    size_t available_ram_pages;
    alloc_static_t *static_heap;

    pmm_page_t *page_metadata;

    pmm_mmap_t mmap;
    /// Same as #mmap, but consists only of available RAM regions.
    pmm_mmap_t alloc_mmap;
} pmm_ctx_t;

static pmm_ctx_t g_pmm;
static pmm_region_t g_pmm_first_region_after_lower_mem;
static pmm_region_t pmm_static_heap_region;

static void prv_pmm_print_mmap(const pmm_mmap_t *mmap);
static const char *prv_pmm_region_type_name(pmm_region_type_t region_type);

static void prv_pmm_reserve_lower_memory(pmm_mmap_t *mmap);
static void prv_pmm_count_available_memory(pmm_ctx_t *pmm);
static void prv_pmm_init_static_heap(pmm_ctx_t *pmm);
static void prv_pmm_init_alloc_mmap(pmm_ctx_t *pmm);
static void prv_pmm_init_page_metadata(pmm_ctx_t *pmm);
static void prv_pmm_init_pools(pmm_ctx_t *pmm);
static void prv_pmm_build_region_pools(pmm_ctx_t *pmm, pmm_region_t *region);
static void prv_pmm_partition_region_pools(pmm_ctx_t *pmm, pmm_region_t *region,
                                           uint32_t start, uint32_t size);
static alloc_buddy_t *prv_pmm_create_pool(pmm_ctx_t *pmm, uint32_t start,
                                          size_t size, uint32_t *out_start,
                                          uint32_t *out_size);

static paddr_t prv_pmm_alloc_in_region(pmm_region_t *region, size_t num_pages,
                                       size_t align_pages);
static paddr_t prv_pmm_alloc_in_pool(alloc_buddy_t *pool, size_t num_pages,
                                     size_t align_pages);

static pmm_region_t *prv_pmm_find_region_by_addr(pmm_ctx_t *pmm, paddr_t addr);
static alloc_buddy_t *prv_pmm_find_pool_by_addr(pmm_region_t *region,
                                                paddr_t addr, size_t num_pages);

static size_t prv_pmm_calc_log2(size_t num) {
    size_t log2 = 0;
    while (num >>= 1) {
        log2++;
    }
    return log2;
}

void pmm_init(const pmm_mmap_t *mmap) {
    // NOTE: this is a shallow copy, the list nodes are not copied.
    kmemcpy(&g_pmm.mmap, mmap, sizeof(*mmap));

    g_pmm.static_heap = heap_get_static_heap();

    prv_pmm_reserve_lower_memory(&g_pmm.mmap);
    prv_pmm_count_available_memory(&g_pmm);
    prv_pmm_init_static_heap(&g_pmm);
    prv_pmm_init_alloc_mmap(&g_pmm);
    prv_pmm_init_page_metadata(&g_pmm);
    prv_pmm_init_pools(&g_pmm);

    kprintf("pmm: memory map upon init exit:\n");
    pmm_print_mmap();

    const size_t static_size =
        g_pmm.static_heap->end - g_pmm.static_heap->start;
    const size_t static_left = g_pmm.static_heap->end - g_pmm.static_heap->next;
    kprintf("pmm: static heap has %u bytes left (%u %%)\n", static_left,
            100 * static_left / static_size);
}

void pmm_print_mmap(void) {
    prv_pmm_print_mmap(&g_pmm.mmap);
}

paddr_t pmm_alloc_pages(size_t num_pages) {
    return pmm_alloc_aligned_pages(num_pages, 1);
}

paddr_t pmm_alloc_aligned_pages(size_t num_pages, size_t align_pages) {
    if (num_pages == 0) {
        kprintf("pmm: requested number of pages is zero\n");
        panic("invalid argument");
    }
    if (align_pages == 0 || (align_pages & (align_pages - 1)) != 0) {
        kprintf("pmm: requested alignment of %u pages is not a power of two\n",
                align_pages);
        panic("invalid argument");
    }

    for (list_node_t *node = g_pmm.alloc_mmap.entry_list.p_first_node;
         node != NULL; node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        const paddr_t addr =
            prv_pmm_alloc_in_region(region, num_pages, align_pages);
        if (addr != 0) {
            // Do an extra check in case ytalloc is buggy.
            if (addr % (PMM_PAGE_SIZE * align_pages) != 0) {
                kprintf("pmm: internal error: returned address 0x%08x is not "
                        "aligned at %u pages\n",
                        addr, align_pages);
                panic("unexpected behavior");
            }
            return addr;
        }
    }

    kprintf("pmm: failed to allocate %u pages aligned at %u pages\n", num_pages,
            align_pages);
    panic("physical memory allocation failed");
}

void pmm_free_pages(paddr_t addr, size_t num_pages) {
    if (addr == 0) { return; }

    pmm_region_t *const region = prv_pmm_find_region_by_addr(&g_pmm, addr);
    if (!region) {
        kprintf("pmm: could not find a region by address 0x%08x\n",
                (uint32_t)addr);
        panic("unexpected behavior");
    }

    alloc_buddy_t *const pool =
        prv_pmm_find_pool_by_addr(region, addr, num_pages);
    if (!pool) {
        kprintf("pmm: could not find the allocation pool for address 0x%08x "
                "size 0x%08x pages\n",
                (uint32_t)addr, num_pages);
        panic("unexpected behavior");
    }

    alloc_buddy_free(pool, (void *)addr, PMM_PAGE_SIZE * num_pages);
}

pmm_page_t *pmm_paddr_to_page(paddr_t addr) {
    const size_t aligned_addr = addr & ~(PMM_PAGE_SIZE - 1);

    const pmm_region_t *const region =
        prv_pmm_find_region_by_addr(&g_pmm, addr);
    if (!region) {
        kprintf("pmm: could not find the region of address 0x%08x\n", addr);
        panic("unexpected behavior");
    }

    const size_t rel_idx = (aligned_addr - region->start) / PMM_PAGE_SIZE;
    const size_t idx = region->page_metadata_offset + rel_idx;

#if 0
    kprintf("pmm: page 0x%08x metadata idx %u\n", addr, idx);
#endif
    return &g_pmm.page_metadata[idx];
}

void pmm_push_page(uint32_t addr) {
    (void)addr;
    panic("not implemented");
}

uint32_t pmm_pop_page(void) {
    panic("not implemented");
    return 0;
}

static void prv_pmm_print_mmap(const pmm_mmap_t *mmap) {
    size_t idx = 0;
    for (list_node_t *node = mmap->entry_list.p_first_node; node != NULL;
         node = node->p_next, idx++) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        kprintf("pmm: entry %u: [0x%08x_%08x; 0x%08x_%08x) type '%s'\n", idx,
                (uint32_t)(region->start >> 32), (uint32_t)region->start,
                (uint32_t)(region->end_incl >> 32), (uint32_t)region->end_incl,
                prv_pmm_region_type_name(region->type));
    }
}

static const char *prv_pmm_region_type_name(pmm_region_type_t region_type) {
    switch (region_type) {
    case PMM_REGION_AVAILABLE:
        return "available RAM";
    case PMM_REGION_RESERVED:
        return "reserved";
    case PMM_REGION_KERNEL_RESERVED:
        return "kernel reserved";
    case PMM_REGION_KERNEL_AND_MODS:
        return "kernel and mods";
    case PMM_REGION_STATIC_HEAP:
        return "static heap";
    default:
        return "<unknown>";
    }
}

/**
 * Types the lower memory as #PMM_REGION_KERNEL_RESERVED.
 *
 * If some memory was already typed as something other than
 * #PMM_REGION_AVAILABLE, it leaves it be.
 *
 * See #PMM_RESERVE_LOWER_BYTES.
 */
static void prv_pmm_reserve_lower_memory(pmm_mmap_t *mmap) {
    if (PMM_RESERVE_LOWER_BYTES == 0) { return; }

    size_t cnt_prev_available = 0;

    // Find the last region that crosses the boundary at
    // PMM_RESERVE_LOWER_BYTES.
    pmm_region_t *last_region;
    LIST_FIND(&mmap->entry_list, last_region, pmm_region_t, node,
              region->end_incl >= PMM_RESERVE_LOWER_BYTES - 1, region);
    if (!last_region) {
        kprintf("pmm: could not find the region that crosses byte 0x%08x\n",
                PMM_RESERVE_LOWER_BYTES);
        panic("unexpected behavior");
    }

    // Iterate backwards from `last_region` and mark every available RAM region
    // as kernel reserved.
    for (list_node_t *node = last_region->node.p_prev; node != NULL;
         node = node->p_prev) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type == PMM_REGION_AVAILABLE) {
#if 0
            kprintf(
                "pmm: region 0x%08x_%08x .. 0x%08x_%08x available, reserving\n",
                (uint32_t)(region->start >> 32), (uint32_t)region->start,
                (uint32_t)(region->end_incl >> 32), (uint32_t)region->end_incl);
#endif
            region->type = PMM_REGION_KERNEL_RESERVED;
            cnt_prev_available += region->end_incl - region->start + 1;
        } else {
#if 0
            kprintf("pmm: region 0x%08x_%08x .. 0x%08x_%08x already not "
                    "available, skipping\n",
                    (uint32_t)(region->start >> 32), (uint32_t)region->start,
                    (uint32_t)(region->end_incl >> 32),
                    (uint32_t)region->end_incl);
#endif
            continue;
        }
    }

    if (last_region->type == PMM_REGION_AVAILABLE) {
        // Split `last_region` into two parts: kernel reserved and available.
        if (last_region->end_incl != PMM_RESERVE_LOWER_BYTES - 1) {
            g_pmm_first_region_after_lower_mem.start = PMM_RESERVE_LOWER_BYTES;
            g_pmm_first_region_after_lower_mem.end_incl = last_region->end_incl;
            g_pmm_first_region_after_lower_mem.type = PMM_REGION_AVAILABLE;

            list_insert(&mmap->entry_list, &last_region->node,
                        &g_pmm_first_region_after_lower_mem.node);
        }

        last_region->end_incl = PMM_RESERVE_LOWER_BYTES - 1;
        last_region->type = PMM_REGION_KERNEL_RESERVED;
        cnt_prev_available += last_region->end_incl - last_region->start + 1;
    }

    kprintf(
        "pmm: reserved lower %u bytes, %u of which were previously available\n",
        PMM_RESERVE_LOWER_BYTES, cnt_prev_available);
}

static void prv_pmm_count_available_memory(pmm_ctx_t *pmm) {
    size_t available_ram_size = 0;

    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        const size_t region_size = region->end_incl - region->start + 1;

        if (region->type == PMM_REGION_AVAILABLE) {
            available_ram_size += region_size;
        }
    }

    pmm->available_ram_size = available_ram_size;
    pmm->available_ram_pages = available_ram_size / PMM_PAGE_SIZE;

    const size_t available_ram_size_kib = available_ram_size / 1024;
    const size_t available_ram_size_mib = available_ram_size_kib / 1024;
    const size_t available_ram_size_gib = available_ram_size_mib / 1024;
    kprintf("pmm: available RAM size: %u B or %u KiB or %u MiB or %u GiB or "
            "%u pages\n",
            available_ram_size, available_ram_size_kib, available_ram_size_mib,
            available_ram_size_gib, pmm->available_ram_pages);
}

static void prv_pmm_init_static_heap(pmm_ctx_t *pmm) {
    /*
     * For each available physical memory region there could be several buddy
     * allocator pools. If the order 0 block is a single page (4 KiB), we need
     * 20 orders to cover 4 GiB.
     *
     * A single buddy allocator pool requires (on a 64-bit system):
     * - alloc_buddy_t (64 bytes on a 64-bit system),
     * - free head pointers array (given 20 orders, 20 * 8 = 160 bytes),
     * - usage bitmap (num_order0_blocks / 8 1/B =
     *                 = region_size / (4 KiB * 8 1/B)).
     *
     * The final formula for the static heap size required for these structures:
     * 224 B + region_size / 32 KiB/B.
     *
     * Approximation of the static heap size required for these structures:
     * - If the RAM size is 16 MiB: 224 B + 16 MiB / 32 KiB/B =~ 0.7 KiB
     *   (0.004%).
     * - 128 MiB: 128 MiB / 32 KiB/B =~ 4 KiB (0.003%).
     * - 4 GiB: 4 GiB / 32 KiB/B =~ 128 KiB (0.003%).
     * - x B: x * 0.003%
     *
     * So we use at least 0.01% of the available RAM for storing these
     * structures.
     *
     * Besides that, the static heap is used for page metadata. Each available
     * RAM page requires a metadata structure. So that adds:
     *
     *   RAM size / 4 KiB * sizeof(pmm_page_t)
     */

    const size_t buddies_size = pmm->available_ram_size / 10000;
    const size_t page_metadata_size =
        pmm->available_ram_pages * sizeof(pmm_page_t);
    const size_t static_heap_size =
        (buddies_size + page_metadata_size + PMM_PAGE_SIZE - 1) &
        ~(PMM_PAGE_SIZE - 1);

    pmm_region_t *found_region;
    LIST_FIND(&pmm->mmap.entry_list, found_region, pmm_region_t, node,
              (region->type == PMM_REGION_AVAILABLE &&
               (region->end_incl - region->start + 1) >= static_heap_size),
              region);
    if (!found_region) {
        kprintf("pmm: could not find an available RAM region for a static heap "
                "of size %u\n",
                static_heap_size);
        panic("out of memory");
    }

    pmm_static_heap_region.start = found_region->start;
    pmm_static_heap_region.end_incl =
        found_region->start + static_heap_size - 1;
    pmm_static_heap_region.type = PMM_REGION_STATIC_HEAP;

    found_region->start += static_heap_size;

    list_insert(&pmm->mmap.entry_list, found_region->node.p_prev,
                &pmm_static_heap_region.node);

    kprintf("pmm: static heap region: 0x%08x_%08x .. 0x%08x_%08x\n",
            (uint32_t)(pmm_static_heap_region.start >> 32),
            (uint32_t)pmm_static_heap_region.start,
            (uint32_t)(pmm_static_heap_region.end_incl >> 32),
            (uint32_t)pmm_static_heap_region.end_incl);

    if (pmm_static_heap_region.end_incl > UINT32_MAX) {
        kprintf("pmm: static heap region end is larger than UINT32_MAX\n");
        panic("not implemented");
    }

    alloc_static_init(pmm->static_heap,
                      (void *)(uintptr_t)pmm_static_heap_region.start,
                      static_heap_size);
}

/**
 * Initialize the available RAM memory map.
 *
 * See #pmm_ctx_t.alloc_mmap.
 */
static void prv_pmm_init_alloc_mmap(pmm_ctx_t *pmm) {
    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type != PMM_REGION_AVAILABLE) { continue; }

        pmm_region_t *const alloc_region =
            alloc_static(pmm->static_heap, sizeof(pmm_region_t));
        if (!alloc_region) {
            kprintf("pmm: failed to statically allocate %u bytes for "
                    "pmm_region_t\n",
                    sizeof(pmm_region_t));
            panic("out of memory");
        }

        kmemcpy(alloc_region, region, sizeof(*region));
        kmemset(&alloc_region->node, 0, sizeof(list_node_t));

        list_append(&pmm->alloc_mmap.entry_list, &alloc_region->node);
    }
}

/**
 * Initializes page metadata.
 *
 * For every available RAM page, there is a metadata structure. This function
 * allocates the storage used by these metadata structures.
 */
static void prv_pmm_init_page_metadata(pmm_ctx_t *pmm) {
    const size_t metadata_size = pmm->available_ram_pages * sizeof(pmm_page_t);
    pmm->page_metadata = alloc_static(pmm->static_heap, metadata_size);
    if (!pmm->page_metadata) {
        kprintf(
            "pmm: failed to statically allocate %u bytes for page metadata\n",
            metadata_size);
        panic("out of memory");
    }

    kprintf("pmm: initializing page metadata\n");
    for (size_t idx = 0; idx < pmm->available_ram_pages; idx++) {
        pmm->page_metadata[idx].type = PMM_PAGE_FREE;
        pmm->page_metadata[idx].reserved = NULL;
    }

    size_t offset = 0;
    for (list_node_t *node = pmm->alloc_mmap.entry_list.p_first_node;
         node != NULL; node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        if (region->start & (PMM_PAGE_SIZE - 1)) {
            kprintf("pmm: region 0x%08x_%08x .. 0x%08x_%08x start is not "
                    "page-aligned\n",
                    (uint32_t)(region->start >> 32), (uint32_t)region->start,
                    (uint32_t)(region->start >> 32), (uint32_t)region->start);
            panic("not implemented");
        }
        if ((region->end_incl + 1) & (PMM_PAGE_SIZE - 1)) {
            kprintf("pmm: region 0x%08x_%08x .. 0x%08x_%08x end is not "
                    "page-aligned\n",
                    (uint32_t)(region->start >> 32), (uint32_t)region->start,
                    (uint32_t)(region->end_incl >> 32),
                    (uint32_t)region->end_incl);
            panic("not implemented");
        }

        const size_t region_size = region->end_incl - region->start + 1;
        const size_t region_num_pages = region_size / PMM_PAGE_SIZE;

        region->page_metadata_offset = offset;
        offset += region_num_pages;
    }
}

/**
 * Initialize allocation pools for each region.
 *
 * Each region may have several allocation pools, the number depending on the
 * region alignment and size.
 */
static void prv_pmm_init_pools(pmm_ctx_t *pmm) {
    for (list_node_t *node = pmm->alloc_mmap.entry_list.p_first_node;
         node != NULL; node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        if (region->end_incl > UINT32_MAX) {
            // TODO: print something?
            continue;
        }

        prv_pmm_build_region_pools(pmm, region);
        kprintf("pmm: region 0x%08x .. 0x%08x has %u buddy pools\n",
                (uint32_t)region->start, (uint32_t)region->end_incl,
                region->num_pools);
    }
}

/**
 * Build allocation pools for the specified region.
 *
 * @warning
 * It is assumed that @a region has the type #PMM_REGION_AVAILABLE.
 */
static void prv_pmm_build_region_pools(pmm_ctx_t *pmm, pmm_region_t *region) {
    const size_t array_size = PMM_MAX_POOLS_PER_REGION * sizeof(void *);
    region->num_pools = 0;
    region->v_pools = alloc_static(pmm->static_heap, array_size);
    if (!region->v_pools) {
        kprintf("pmm: failed to statically allocate %u bytes for the pool "
                "pointers array\n",
                array_size);
        panic("out of memory");
    }

    prv_pmm_partition_region_pools(pmm, region, region->start,
                                   region->end_incl + 1);
}

/**
 * Partition the region into several pools.
 *
 * This is a recursive function. It uses #prv_pmm_create_pool() to create a
 * single pool covering some part in the specified range `[start; end_excl)`,
 * and then calls itself two times for the remaining two parts of the range.
 */
static void prv_pmm_partition_region_pools(pmm_ctx_t *pmm, pmm_region_t *region,
                                           uint32_t start, uint32_t end_excl) {
    const size_t size = end_excl - start;
    if (size < PMM_MIN_POOL_SIZE) { return; }
    if (region->num_pools >= PMM_MAX_POOLS_PER_REGION) { return; }

#if 0
    kprintf("pmm: init pools for region 0x%08x .. 0x%08x\n", start,
            start + size - 1);
#endif

    uint32_t used_start;
    uint32_t used_size;
    alloc_buddy_t *const pool =
        prv_pmm_create_pool(pmm, start, size, &used_start, &used_size);

    alloc_buddy_t **const pools = region->v_pools;
    pools[region->num_pools] = pool;
    region->num_pools++;

    prv_pmm_partition_region_pools(pmm, region, start, used_start);
    prv_pmm_partition_region_pools(pmm, region, used_start + used_size,
                                   end_excl);
}

/**
 * Create a pool in the specified range.
 *
 * @param       pmm             PMM context struct.
 * @param       start           Lower address limit for the pool to be created.
 * @param       size            Size limit for the pool to be created.
 * @param[out]  used_start      Created pool start address.
 * @param[out]  used_size       Created pool size.
 *
 * @returns A pointer to the created buddy allocator, if it could fit into the
 * specified memory range and be larger than #PMM_MIN_POOL_SIZE, otherwise
 * `NULL`.
 *
 * @note
 * Because the underlying allocator library requires the pool start address to
 * be aligned at its size, the used start address and the used size are almost
 * always different from @a start and @a size.
 */
static alloc_buddy_t *prv_pmm_create_pool(pmm_ctx_t *pmm, uint32_t start,
                                          size_t size, uint32_t *used_start,
                                          uint32_t *used_size) {
    const uint32_t end = start + size;

    size_t aligned_start = 0;
    size_t block_size = 0;
    size_t log2_size = prv_pmm_calc_log2(size);
    if (log2_size < PMM_LOG2_MIN_POOL_SIZE) { return NULL; }
    while (log2_size >= PMM_LOG2_MIN_POOL_SIZE) {
        block_size = 1 << log2_size;
        aligned_start = (start + (block_size - 1)) & ~(block_size - 1);
        if (aligned_start + block_size <= end) { break; }
        log2_size--;
    }

    if (used_start) { *used_start = aligned_start; }
    if (used_size) { *used_size = block_size; }

    alloc_buddy_t *const heap = alloc_static(pmm->static_heap, sizeof(*heap));
    if (!heap) {
        kprintf("pmm: failed to statically allocate %u bytes for "
                "alloc_buddy_t\n",
                sizeof(*heap));
        panic("out of memory");
    }

    const size_t free_heads_size = PMM_BUDDY_MAX_ORDERS * sizeof(uintptr_t);
    void *const free_heads = alloc_static(pmm->static_heap, free_heads_size);
    if (!free_heads) {
        kprintf("pmm: failed to statically allocate %u bytes for free "
                "heads buffer\n",
                free_heads_size);
        panic("out of memory");
    }

    const size_t num_order0_blocks = block_size / YTALLOC_BUDDY_MIN_BLOCK_SIZE;
    const size_t bitmap_size = ((num_order0_blocks + 7) & ~7) / 8;
    void *const bitmap = alloc_static(pmm->static_heap, bitmap_size);
    if (!bitmap) {
        kprintf("pmm: failed to statically allocate %u bytes for bitmap "
                "buffer\n",
                bitmap_size);
        panic("out of memory");
    }

#if 0
    kprintf("pmm: init pool at 0x%08x size 0x%08x\n", aligned_start,
            block_size);
#endif
    alloc_buddy_init(heap, (void *)aligned_start, block_size, free_heads,
                     free_heads_size, bitmap, bitmap_size);

    return heap;
}

static paddr_t prv_pmm_alloc_in_region(pmm_region_t *region, size_t num_pages,
                                       size_t align_pages) {
    alloc_buddy_t **const pools = region->v_pools;

    for (size_t idx = 0; idx < region->num_pools; idx++) {
        alloc_buddy_t *const pool = pools[idx];
        const paddr_t addr =
            prv_pmm_alloc_in_pool(pool, num_pages, align_pages);
        if (addr != 0) { return addr; }
    }

    return 0;
}

static paddr_t prv_pmm_alloc_in_pool(alloc_buddy_t *pool, size_t num_pages,
                                     size_t align_pages) {
    const size_t size = PMM_PAGE_SIZE * num_pages;
    const size_t align = PMM_PAGE_SIZE * align_pages;
    return (paddr_t)alloc_buddy_aligned(pool, size, align);
}

static pmm_region_t *prv_pmm_find_region_by_addr(pmm_ctx_t *pmm, paddr_t addr) {
    for (list_node_t *node = pmm->alloc_mmap.entry_list.p_first_node;
         node != NULL; node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        if (region->start <= (uint64_t)addr &&
            (uint64_t)addr <= region->end_incl) {
            return region;
        }
    }

    return NULL;
}

static alloc_buddy_t *prv_pmm_find_pool_by_addr(pmm_region_t *region,
                                                paddr_t addr,
                                                size_t num_pages) {
    const paddr_t end_excl = addr + PMM_PAGE_SIZE * num_pages;
    alloc_buddy_t **const pools = region->v_pools;

    for (size_t idx = 0; idx < region->num_pools; idx++) {
        alloc_buddy_t *const pool = pools[idx];
        if (pool->start <= addr && end_excl <= pool->end) { return pool; }
    }

    return NULL;
}
