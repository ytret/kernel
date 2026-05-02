/**
 * @file pmm.c
 *
 * Physical memory manager.
 *
 * The PMM starts from the bootloader-provided memory map, patches holes in it,
 * inserts new kernel regions, and then serves page allocations from the
 * remaining available RAM.
 *
 * Allocation is pool-based. Each available memory region is partitioned into
 * one or more buddy-allocator-backed pools, and page requests are satisfied by
 * iterating over the regions and their pools until a pool that has enough free
 * space for the allocation is found.
 *
 * Glossary:
 * - region: a contiguous physical address range from the PMM memory map, tagged
 *   with a type (see #pmm_region_type_t);
 * - pool: a power-of-two-sized part of an available region, managed by a single
 *   buddy allocator;
 * - early page allocator: a static allocator backed by its own
 *   reserved region, used during bootstrap to allocate pages needed to map and
 *   initialize the page-table-provider pool;
 * - page-table-provider pool: the first mapped and initialized pool; it is used
 *   to map all the other pools and can later, like any other pool, satisfy
 *   regular allocations.
 *
 * Pool initialization has three phases:
 * 1. Build pool descriptors by partitioning each available region into
 *    power-of-two-sized buddy pools. The data structures are allocated on the
 *    static heap.
 * 2. Select one pool to bootstrap page-table allocations. Map and initialize
 *    it. The page tables are allocated using the early page allocator.
 * 3. Map and initialize the remaining pools. The page tables are allocated
 *    using the page-table-provider pool.
 */

#include <ytalloc/ytalloc.h>

#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "heap.h"
#include "kinttypes.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"
#include "vmm.h"

#define PMM_RESERVE_LOWER_BYTES (4U * 1024U * 1024U)

/**
 * Maximum number of patch regions.
 *
 * A patch region is constructed to cover an empty range in the memory map. If
 * this number is too low to cover every memory map hole, the kernel panics.
 */
#define PMM_MAX_PATCH_REGIONS 16

#define PMM_BUDDY_MAX_ORDERS     20
#define PMM_LOG2_MIN_POOL_SIZE   22 // log2 of 4 MiB
#define PMM_MIN_POOL_SIZE        (1 << PMM_LOG2_MIN_POOL_SIZE)
#define PMM_MAX_POOLS_PER_REGION 16

/**
 * Size of the early page allocator in pages.
 *
 * The early page allocator is used for mapping the page table provider pool,
 * which is in turn used to map all the other pools. Size of memory available to
 * this allocator must be enough to map the page table pool.
 *
 * TODO: determine this value dynamically depending on the pool size.
 */
#define PMM_EARLY_PGALLOC_PAGES 64

typedef struct {
    alloc_buddy_t *alloc;
    void *v_start;
    size_t size;
    void *free_heads;
    size_t free_heads_size;
    void *bitmap;
    size_t bitmap_size;
} pmm_pool_t;

typedef struct {
    size_t available_ram_size;
    size_t available_ram_pages;

    alloc_static_t *static_heap;
    alloc_static_t early_pgalloc;
    bool early_pgalloc_ready;
    pmm_pool_t *pgtbl_prov_pool;

    pmm_page_t *page_metadata;

    pmm_mmap_t mmap;
} pmm_ctx_t;

static pmm_ctx_t g_pmm;
static pmm_region_t g_pmm_first_region_after_lower_mem;
static pmm_region_t pmm_static_heap_region;
static pmm_region_t pmm_early_pgalloc_rgn;
static pmm_region_t g_pmm_patch_regions[PMM_MAX_PATCH_REGIONS];
static size_t g_pmm_patch_region_cnt;

static void prv_pmm_dump_mmap(const pmm_mmap_t *mmap);
static const char *prv_pmm_region_type_name(pmm_region_type_t region_type);

static void prv_pmm_patch_mmap_holes(pmm_mmap_t *mmap);
static void prv_pmm_reserve_lower_mem(pmm_mmap_t *mmap);
static void prv_pmm_count_avail_ram(pmm_ctx_t *pmm);
static void prv_pmm_init_static_heap(pmm_ctx_t *pmm);
static void prv_pmm_init_early_pgalloc(pmm_ctx_t *pmm);
static void prv_pmm_init_page_metadata(pmm_ctx_t *pmm);
static void prv_pmm_build_pools(pmm_ctx_t *pmm);
static void prv_pmm_build_region_pools(pmm_ctx_t *pmm, pmm_region_t *region);
static void prv_pmm_partition_pools(pmm_ctx_t *pmm, pmm_region_t *region,
                                    uint32_t start, uint32_t size);
static pmm_pool_t *prv_pmm_build_pool(pmm_ctx_t *pmm, uint32_t start,
                                      size_t size, uint32_t *out_start,
                                      uint32_t *out_size);
static void prv_pmm_init_pools(pmm_ctx_t *pmm);
static void prv_pmm_init_pgtbl_pool(pmm_ctx_t *pmm);

static paddr_t prv_pmm_alloc_in_region(pmm_region_t *region, size_t num_pages,
                                       size_t align_pages);
static paddr_t prv_pmm_alloc_in_pool(alloc_buddy_t *pool, size_t num_pages,
                                     size_t align_pages);

static void *prv_pmm_early_alloc(size_t size);
static void *prv_pmm_alloc_in_pgtbl_prov(size_t size);

static alloc_buddy_t *prv_pmm_find_alloc_by_addr(pmm_region_t *region,
                                                 paddr_t addr,
                                                 size_t num_pages);

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

    LOG_DEBUG("memory map upon init entry:");
    pmm_dump_mmap();

    prv_pmm_patch_mmap_holes(&g_pmm.mmap);
    prv_pmm_reserve_lower_mem(&g_pmm.mmap);
    prv_pmm_count_avail_ram(&g_pmm);
    prv_pmm_init_static_heap(&g_pmm);
    prv_pmm_init_early_pgalloc(&g_pmm);
    prv_pmm_init_page_metadata(&g_pmm);
    prv_pmm_build_pools(&g_pmm);
    prv_pmm_init_pools(&g_pmm);

    LOG_DEBUG("memory map upon init exit:");
    pmm_dump_mmap();

    const size_t static_size =
        g_pmm.static_heap->end - g_pmm.static_heap->start;
    const size_t static_left = g_pmm.static_heap->end - g_pmm.static_heap->next;
    LOG_DEBUG("static heap has %zu bytes left (%zu %% free)", static_left,
              100 * static_left / static_size);
}

const pmm_mmap_t *pmm_get_mmap(void) {
    return &g_pmm.mmap;
}

void pmm_dump_mmap(void) {
    prv_pmm_dump_mmap(&g_pmm.mmap);
}

bool pmm_mmap_insert(pmm_mmap_t *mmap, pmm_region_t *insert_region,
                     pmm_region_t *leftover) {
    // It is assumed that [region_start, region_end_excl) are residing in a
    // single memory map entry. Otherwise this function will fail.
    pmm_region_t *found_region;
    LIST_FIND(&mmap->entry_list, found_region, pmm_region_t, node,
              region->start <= insert_region->start &&
                  insert_region->end_incl <= region->end_incl,
              region);
    if (found_region) {
        LOG_FLOW("found covering region physical 0x%016llx..0x%016llx",
                 found_region->start, found_region->end_incl);
    } else {
        PANIC("TODO: no single region covers [0x%016llx; 0x%016llx)",
              insert_region->start, insert_region->end_incl);
    }

    LOG_FLOW("insert region physical 0x%016llx..0x%016llx",
             insert_region->start, insert_region->end_incl);

    if (found_region->start == insert_region->start) {
        PANIC("TODO: case when region starts at found region start");
    } else if (found_region->end_incl == insert_region->end_incl) {
        PANIC("TODO: case when region ends at found region end");
    } else {
        // Before: [          found_region          ]
        // After:  [found_region][region][cut_region]
        LOG_FLOW("split found region into three parts");

        leftover->start = insert_region->end_incl + 1;
        leftover->end_incl = found_region->end_incl;

        found_region->end_incl = insert_region->start - 1;

        list_insert(&mmap->entry_list, found_region->node.p_next,
                    &leftover->node);
        list_insert(&mmap->entry_list, &leftover->node, &insert_region->node);

        LOG_FLOW("first part  0x%016llx..0x%016llx", found_region->start,
                 found_region->end_incl);
        LOG_FLOW("inserted pt 0x%016llx..0x%016llx", insert_region->start,
                 insert_region->end_incl);
        LOG_FLOW("third part  0x%016llx..0x%016llx", leftover->start,
                 leftover->end_incl);

        return true;
    }
}

paddr_t pmm_alloc_pages(size_t num_pages) {
    return pmm_alloc_aligned_pages(num_pages, 1);
}

paddr_t pmm_alloc_aligned_pages(size_t num_pages, size_t align_pages) {
    if (num_pages == 0) { PANIC("invalid argument 'num_pages' value 0"); }
    if (align_pages == 0 || (align_pages & (align_pages - 1)) != 0) {
        PANIC("invalid argument 'align_pages' value %zu - not power of two",
              align_pages);
    }

    for (list_node_t *node = g_pmm.mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type != PMM_REGION_AVAILABLE) { continue; }

        const paddr_t addr =
            prv_pmm_alloc_in_region(region, num_pages, align_pages);
        if (addr != 0) {
            // Do an extra check in case ytalloc is buggy.
            if (addr % (PMM_PAGE_SIZE * align_pages) != 0) {
                PANIC("internal error - returned address 0x%016llx is not "
                      "aligned at %zu pages",
                      addr, align_pages);
            }
            return addr;
        }
    }

    LOG_ERROR("failed to allocate %zu pages aligned at %zu pages", num_pages,
              align_pages);
    PANIC("physical memory allocation failed");
}

void pmm_free_pages(paddr_t addr, size_t num_pages) {
    if (addr == 0) { return; }

    pmm_region_t *const region = pmm_find_region_by_addr(addr);
    if (!region) {
        PANIC("no physical region contains 0x%08" PRIx32, (uint32_t)addr);
    }

    alloc_buddy_t *const alloc =
        prv_pmm_find_alloc_by_addr(region, addr, num_pages);
    if (!alloc) {
        PANIC("no pool owns allocation at 0x%08" PRIx32 " of %zu pages",
              (uint32_t)addr, num_pages);
    }

    alloc_buddy_free(alloc, (void *)(uintptr_t)addr, PMM_PAGE_SIZE * num_pages);
}

pmm_page_t *pmm_paddr_to_page(paddr_t addr) {
    const size_t aligned_addr = addr & ~(PMM_PAGE_SIZE - 1);

    const pmm_region_t *const region = pmm_find_region_by_addr(addr);
    if (!region) { PANIC("no region contains address 0x%016llx", addr); }

    const size_t rel_idx = (aligned_addr - region->start) / PMM_PAGE_SIZE;
    const size_t idx = region->page_metadata_offset + rel_idx;

    LOG_FLOW("page 0x%08" PRIx32 " metadata idx %zu", (uint32_t)addr, idx);
    return &g_pmm.page_metadata[idx];
}

pmm_region_t *pmm_find_region_by_addr(paddr_t addr) {
    for (list_node_t *node = g_pmm.mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        if (region->start <= (uint64_t)addr &&
            (uint64_t)addr <= region->end_incl) {
            return region;
        }
    }

    return NULL;
}

alloc_static_t *pmm_early_pgalloc(void) {
    if (!g_pmm.early_pgalloc_ready) {
        PANIC("early pgalloc is not initialized");
    }
    return &g_pmm.early_pgalloc;
}

void pmm_push_page(uint32_t addr) {
    (void)addr;
    PANIC("not implemented");
}

uint32_t pmm_pop_page(void) {
    PANIC("not implemented");
    return 0;
}

static void prv_pmm_dump_mmap(const pmm_mmap_t *mmap) {
    size_t idx = 0;
    for (list_node_t *node = mmap->entry_list.p_first_node; node != NULL;
         node = node->p_next, idx++) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

        LOG_DEBUG("%2zu: [0x%016llx; 0x%016llx) type '%s'", idx, region->start,
                  region->end_incl, prv_pmm_region_type_name(region->type));
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

static void prv_pmm_patch_mmap_holes(pmm_mmap_t *mmap) {
    while (true) {
        // We cannot use a single for loop to iterate over the list, because the
        // list changes if there are holes being patched.

        bool is_first_region = true;
        uint64_t last_end_incl = 0;
        bool has_no_holes = true;

        for (list_node_t *node = mmap->entry_list.p_first_node; node != NULL;
             node = node->p_next) {
            pmm_region_t *const region =
                LIST_NODE_TO_STRUCT(node, pmm_region_t, node);

            if (!is_first_region && region->start > last_end_incl + 1) {
                const uint64_t hole_start = last_end_incl + 1;
                const uint64_t hole_end_incl = region->start - 1;
                LOG_DEBUG("found memory map hole 0x%016llx..0x%016llx",
                          hole_start, hole_end_incl);

                if (g_pmm_patch_region_cnt >= PMM_MAX_PATCH_REGIONS) {
                    PANIC("too many holes in physical memory map (%zu > %u)",
                          g_pmm_patch_region_cnt, PMM_MAX_PATCH_REGIONS);
                }

                pmm_region_t *const patch =
                    &g_pmm_patch_regions[g_pmm_patch_region_cnt++];
                patch->start = hole_start;
                patch->end_incl = hole_end_incl;
                patch->type = PMM_REGION_RESERVED;

                list_insert(&mmap->entry_list, node->p_prev, &patch->node);

                has_no_holes = false;
                break;
            }

            is_first_region = false;
            last_end_incl = region->end_incl;
        }

        if (has_no_holes) { break; }
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
static void prv_pmm_reserve_lower_mem(pmm_mmap_t *mmap) {
    if (PMM_RESERVE_LOWER_BYTES == 0) { return; }

    size_t cnt_prev_available = 0;

    // Find the last region that crosses the boundary at
    // PMM_RESERVE_LOWER_BYTES.
    pmm_region_t *last_region;
    LIST_FIND(&mmap->entry_list, last_region, pmm_region_t, node,
              region->end_incl >= PMM_RESERVE_LOWER_BYTES - 1, region);
    if (!last_region) {
        PANIC("no region crosses address 0x%08x", PMM_RESERVE_LOWER_BYTES);
    }

    // Iterate backwards from `last_region` and mark every available RAM region
    // as kernel reserved.
    for (list_node_t *node = last_region->node.p_prev; node != NULL;
         node = node->p_prev) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type == PMM_REGION_AVAILABLE) {
            LOG_FLOW("region 0x%016llx..0x%016llx available, reserving",
                     region->start, region->end_incl);
            region->type = PMM_REGION_KERNEL_RESERVED;
            cnt_prev_available += region->end_incl - region->start + 1;
        } else {
            LOG_FLOW(
                "region 0x%016llx..0x%016llx already not available, skipping",
                region->start, region->end_incl);
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

    LOG_DEBUG("reserved lower %u bytes, %zu of which were previously available",
              PMM_RESERVE_LOWER_BYTES, cnt_prev_available);
}

static void prv_pmm_count_avail_ram(pmm_ctx_t *pmm) {
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
    LOG_INFO("available RAM size: %zu B or %zu KiB or %zu MiB or %zu GiB or "
             "%zu pages",
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
        PANIC("no available RAM region for static heap of size %zu",
              static_heap_size);
    }

    pmm_static_heap_region.start = found_region->start;
    pmm_static_heap_region.end_incl =
        found_region->start + static_heap_size - 1;
    pmm_static_heap_region.type = PMM_REGION_STATIC_HEAP;

    found_region->start += static_heap_size;

    list_insert(&pmm->mmap.entry_list, found_region->node.p_prev,
                &pmm_static_heap_region.node);

    const uint64_t virt_start = PHYS_TO_VIRT(pmm_static_heap_region.start);
    const uint64_t virt_end_incl =
        PHYS_TO_VIRT(pmm_static_heap_region.end_incl);

    LOG_DEBUG("static heap region: physical 0x%016llx..0x%016llx",
              pmm_static_heap_region.start, pmm_static_heap_region.end_incl);
    LOG_DEBUG("static heap region: virtual  0x%016llx..0x%016llx", virt_start,
              virt_end_incl);

    if (virt_start > VADDR_MAX) {
        PANIC("static heap region virtual start 0x%16llx is larger than "
              "VADDR_MAX",
              virt_start);
    }
    if (virt_end_incl > VADDR_MAX) {
        PANIC(
            "static heap region virtual end 0x%16llx is larger than VADDR_MAX",
            virt_end_incl);
    }

    alloc_static_init(pmm->static_heap, (void *)(vaddr_t)virt_start,
                      static_heap_size);
}

static void prv_pmm_init_early_pgalloc(pmm_ctx_t *pmm) {
    const size_t pgalloc_size = PMM_EARLY_PGALLOC_PAGES * PMM_PAGE_SIZE;

    pmm_region_t *found_region;
    LIST_FIND(&pmm->mmap.entry_list, found_region, pmm_region_t, node,
              (region->type == PMM_REGION_AVAILABLE &&
               (region->end_incl - region->start + 1) >= pgalloc_size),
              region);
    if (!found_region) {
        PANIC("no available RAM region for early page allocator of size %zu",
              pgalloc_size);
    }

    pmm_early_pgalloc_rgn.start = found_region->start;
    pmm_early_pgalloc_rgn.end_incl = found_region->start + pgalloc_size - 1;
    pmm_early_pgalloc_rgn.type = PMM_REGION_STATIC_HEAP;

    found_region->start += pgalloc_size;

    list_insert(&pmm->mmap.entry_list, found_region->node.p_prev,
                &pmm_early_pgalloc_rgn.node);

    const uint64_t virt_start = PHYS_TO_VIRT(pmm_early_pgalloc_rgn.start);
    const uint64_t virt_end_incl = PHYS_TO_VIRT(pmm_early_pgalloc_rgn.end_incl);

    LOG_DEBUG("early pgalloc region: physical 0x%016llx..0x%016llx",
              pmm_early_pgalloc_rgn.start, pmm_early_pgalloc_rgn.end_incl);
    LOG_DEBUG("early pgalloc region: virtual  0x%016llx..0x%016llx", virt_start,
              virt_end_incl);

    if (virt_start > VADDR_MAX) {
        PANIC("early pgalloc region virtual start 0x%16llx is larger than "
              "VADDR_MAX",
              virt_start);
    }
    if (virt_end_incl > VADDR_MAX) {
        PANIC("early pgalloc region virtual end 0x%16llx is larger than "
              "VADDR_MAX",
              virt_end_incl);
    }

    if (virt_start & (PMM_PAGE_SIZE - 1)) {
        PANIC("early pgalloc region start must be page-aligned");
    }

    alloc_static_init(&pmm->early_pgalloc, (void *)(vaddr_t)virt_start,
                      pgalloc_size);
    pmm->early_pgalloc_ready = true;
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
        PANIC("failed to statically allocate %zu bytes for page metadata",
              metadata_size);
    }

    LOG_DEBUG("initializing page metadata");
    for (size_t idx = 0; idx < pmm->available_ram_pages; idx++) {
        pmm->page_metadata[idx].type = PMM_ALLOC_NONE;
        pmm->page_metadata[idx].reserved = NULL;
    }

    size_t offset = 0;
    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type != PMM_REGION_AVAILABLE) { continue; }

        if (region->start & (PMM_PAGE_SIZE - 1)) {
            PANIC("TODO: region 0x%016llx..0x%016llx start is not page-aligned",
                  region->start, region->end_incl);
        }
        if ((region->end_incl + 1) & (PMM_PAGE_SIZE - 1)) {
            PANIC("TODO: region 0x%016llx..0x%016llx end is not page-aligned",
                  region->start, region->end_incl);
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
static void prv_pmm_build_pools(pmm_ctx_t *pmm) {
    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        if (region->type != PMM_REGION_AVAILABLE) { continue; }

        if (region->end_incl > UINT32_MAX) {
            // TODO: print something?
            continue;
        }

        prv_pmm_build_region_pools(pmm, region);
        LOG_DEBUG("region 0x%016llx..0x%016llx has %zu buddy pools",
                  region->start, region->end_incl, region->num_pools);
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
        PANIC("failed to statically allocate %zu bytes for pool pointers array",
              array_size);
    }

    prv_pmm_partition_pools(pmm, region, region->start, region->end_incl + 1);
}

/**
 * Partition the region into several pools.
 *
 * This is a recursive function. It uses #prv_pmm_build_pool() to create a
 * single pool covering some part in the specified range `[start; end_excl)`,
 * and then calls itself two times for the remaining two parts of the range.
 */
static void prv_pmm_partition_pools(pmm_ctx_t *pmm, pmm_region_t *region,
                                    uint32_t start, uint32_t end_excl) {
    const size_t size = end_excl - start;
    if (size < PMM_MIN_POOL_SIZE) { return; }
    if (region->num_pools >= PMM_MAX_POOLS_PER_REGION) { return; }

    LOG_DEBUG("init pools for region 0x%08zx..0x%08zx", start,
              start + size - 1);

    uint32_t used_start;
    uint32_t used_size;
    pmm_pool_t *const pool =
        prv_pmm_build_pool(pmm, start, size, &used_start, &used_size);

    pmm_pool_t **const pools = region->v_pools;
    pools[region->num_pools] = pool;
    region->num_pools++;

    prv_pmm_partition_pools(pmm, region, start, used_start);
    prv_pmm_partition_pools(pmm, region, used_start + used_size, end_excl);
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
static pmm_pool_t *prv_pmm_build_pool(pmm_ctx_t *pmm, uint32_t start,
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

    pmm_pool_t *const pool = alloc_static(pmm->static_heap, sizeof(*pool));
    if (!pool) {
        PANIC("failed to statically allocate %zu bytes for pmm_pool_t",
              sizeof(*pool));
    }
    kmemset(pool, 0, sizeof(*pool));

    alloc_buddy_t *const heap = alloc_static(pmm->static_heap, sizeof(*heap));
    if (!heap) {
        PANIC("failed to statically allocate %zu bytes for alloc_buddy_t",
              sizeof(*heap));
    }

    const size_t free_heads_size = PMM_BUDDY_MAX_ORDERS * sizeof(uintptr_t);
    void *const free_heads = alloc_static(pmm->static_heap, free_heads_size);
    if (!free_heads) {
        PANIC("failed to statically allocate %zu bytes for free heads buffer",
              free_heads_size);
    }

    const size_t num_order0_blocks = block_size / YTALLOC_BUDDY_MIN_BLOCK_SIZE;
    const size_t bitmap_size = ((num_order0_blocks + 7) & ~7) / 8;
    void *const bitmap = alloc_static(pmm->static_heap, bitmap_size);
    if (!bitmap) {
        PANIC("failed to statically allocate %zu bytes for bitmap buffer",
              bitmap_size);
    }

    LOG_DEBUG("create pool at 0x%08zx size 0x%08zx", aligned_start, block_size);
    pool->alloc = heap;
    pool->v_start = (void *)aligned_start;
    pool->size = block_size;
    pool->free_heads = free_heads;
    pool->free_heads_size = free_heads_size;
    pool->bitmap = bitmap;
    pool->bitmap_size = bitmap_size;

    return pool;
}

static void prv_pmm_init_pools(pmm_ctx_t *pmm) {
    prv_pmm_init_pgtbl_pool(pmm);

    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const rgn = LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        pmm_pool_t **const pools = rgn->v_pools;

        for (size_t idx = 0; idx < rgn->num_pools; idx++) {
            pmm_pool_t *const pool = pools[idx];
            if (pool == pmm->pgtbl_prov_pool) { continue; }

            LOG_DEBUG("map and init pool 0x%08" PRIxPTR " size 0x%08zx",
                      (uintptr_t)pool->v_start, pool->size);
            vmm_kmap_region_a(prv_pmm_alloc_in_pgtbl_prov,
                              (vaddr_t)pool->v_start, pool->size);
            alloc_buddy_init(pool->alloc, pool->v_start, pool->size,
                             pool->free_heads, pool->free_heads_size,
                             pool->bitmap, pool->bitmap_size);
        }
    }
}

static void prv_pmm_init_pgtbl_pool(pmm_ctx_t *pmm) {
    pmm_pool_t *pgtbl_prov = NULL;
    for (list_node_t *node = pmm->mmap.entry_list.p_first_node; node != NULL;
         node = node->p_next) {
        pmm_region_t *const region =
            LIST_NODE_TO_STRUCT(node, pmm_region_t, node);
        pmm_pool_t **const pools = region->v_pools;

        for (size_t idx = 0; idx < region->num_pools; idx++) {
            pmm_pool_t *const pool = pools[idx];
            // TODO: add selection criteria
            // TODO: do this one time?
            LOG_DEBUG("use pool at 0x%08" PRIxPTR " size %zu for page tables",
                      (uintptr_t)pool->v_start, pool->size);
            pgtbl_prov = pool;
            break;
        }
    }
    if (!pgtbl_prov) {
        PANIC("no pool for page tables for mapping other pools");
    }

    vmm_kmap_region_a(prv_pmm_early_alloc, (vaddr_t)pgtbl_prov->v_start,
                      pgtbl_prov->size);

    LOG_DEBUG("init page table pool");
    alloc_buddy_init(pgtbl_prov->alloc, pgtbl_prov->v_start, pgtbl_prov->size,
                     pgtbl_prov->free_heads, pgtbl_prov->free_heads_size,
                     pgtbl_prov->bitmap, pgtbl_prov->bitmap_size);

    pmm->pgtbl_prov_pool = pgtbl_prov;
}

static paddr_t prv_pmm_alloc_in_region(pmm_region_t *region, size_t num_pages,
                                       size_t align_pages) {
    pmm_pool_t **const pools = region->v_pools;

    for (size_t idx = 0; idx < region->num_pools; idx++) {
        pmm_pool_t *const pool = pools[idx];
        const paddr_t addr =
            prv_pmm_alloc_in_pool(pool->alloc, num_pages, align_pages);
        if (addr != 0) { return addr; }
    }

    return 0;
}

static paddr_t prv_pmm_alloc_in_pool(alloc_buddy_t *pool, size_t num_pages,
                                     size_t align_pages) {
    const size_t size = PMM_PAGE_SIZE * num_pages;
    const size_t align = PMM_PAGE_SIZE * align_pages;
    return (paddr_t)(uintptr_t)alloc_buddy_aligned(pool, size, align);
}

static void *prv_pmm_early_alloc(size_t size) {
    if (size % PMM_PAGE_SIZE != 0) {
        PANIC(
            "invalid argument 'size' value %zu, it must be aligned at %u bytes",
            size, PMM_PAGE_SIZE);
    }

    alloc_static_t *const early_pgalloc = pmm_early_pgalloc();
    void *const page = alloc_static(early_pgalloc, size);

    if (!page) {
        PANIC("early pgalloc (total size %" PRIuPTR
              ") failed to allocate %zu bytes",
              early_pgalloc->end - early_pgalloc->start, size);
    }

    if ((uintptr_t)page & (PMM_PAGE_SIZE - 1)) {
        PANIC("early pgalloc returned non-page-aligned address 0x%08" PRIxPTR,
              (uintptr_t)page);
    }

    return page;
}

static void *prv_pmm_alloc_in_pgtbl_prov(size_t size) {
    if (!g_pmm.pgtbl_prov_pool) {
        PANIC("page table provider pool is not initialized");
    }

    return alloc_buddy_aligned(g_pmm.pgtbl_prov_pool->alloc, size,
                               PMM_PAGE_SIZE);
}

static alloc_buddy_t *prv_pmm_find_alloc_by_addr(pmm_region_t *region,
                                                 paddr_t addr,
                                                 size_t num_pages) {
    const paddr_t end_excl = addr + PMM_PAGE_SIZE * num_pages;
    pmm_pool_t **const pools = region->v_pools;

    LOG_FLOW(
        "find addr 0x%016llx in region 0x%016llx..0x%016llx with %zu pools",
        addr, region->start, region->end_incl, region->num_pools);

    for (size_t idx = 0; idx < region->num_pools; idx++) {
        pmm_pool_t *const pool = pools[idx];
        alloc_buddy_t *const alloc = pool->alloc;
        LOG_FLOW("pool %zu alloc start 0x%08" PRIxPTR " end 0x%08" PRIxPTR, idx,
                 alloc->start, alloc->end);
        if (alloc->start <= addr && end_excl <= alloc->end) { return alloc; }
    }

    return NULL;
}
