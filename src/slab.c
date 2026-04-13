#include <ytalloc/ytalloc.h>

#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "assert.h"
#include "kinttypes.h"
#include "log.h"
#include "memfun.h"
#include "pmm.h"
#include "slab.h"

#define SLAB_MIN_ITEMS_PER_SLAB 2

typedef struct {
    list_node_t node;
    alloc_slab_t alloc;
    slab_cache_t *cache; //!< Pointer to the parent cache.
} slab_t;

static slab_t *prv_slab_get_for_alloc(slab_cache_t *cache);
static slab_t *prv_slab_new(slab_cache_t *cache, size_t num_items);

void slab_init_cache(slab_cache_t *cache, size_t item_size) {
    kmemset(cache, 0, sizeof(*cache));

    cache->item_size = item_size;

    list_init(&cache->free, NULL);
    list_init(&cache->partial, NULL);
    list_init(&cache->full, NULL);
}

void *slab_alloc(slab_cache_t *cache) {
    slab_t *const slab = prv_slab_get_for_alloc(cache);
    return alloc_slab(&slab->alloc);
}

void slab_free(void *v_slab, void *ptr) {
    if (!v_slab) { PANIC("invalid argument 'v_slab' value NULL"); }
    if (!ptr) { return; }

    LOG_FLOW("free item at 0x%08" PRIxPTR " of slab 0x%08" PRIxPTR,
             (uintptr_t)ptr, (uintptr_t)v_slab);

    slab_t *const slab = v_slab;
    alloc_slab_free(&slab->alloc, ptr);

    slab_cache_t *const cache = slab->cache;
    const size_t used_items = alloc_slab_num_used(&slab->alloc);
    if (used_items == slab->alloc.num_items - 1) {
        // full -> partial
        LOG_FLOW("cache %zu slab 0x%08" PRIxPTR " move full -> partial",
                 cache->item_size, (uintptr_t)slab);
        list_remove(&cache->full, &slab->node);
        list_append(&cache->partial, &slab->node);
    } else if (used_items == 0) {
        // partial -> free
        LOG_FLOW("cache %zu slab 0x%08" PRIxPTR " move partial -> free",
                 cache->item_size, (uintptr_t)slab);
        list_remove(&cache->partial, &slab->node);
        list_append(&cache->free, &slab->node);
    }
}

size_t slab_item_size(const void *v_slab) {
    if (!v_slab) { PANIC("invalid argument 'v_slab' value NULL"); }

    const slab_t *const slab = v_slab;
    if (!slab->cache) { PANIC("slab %p cache is NULL", slab); }

    return slab->cache->item_size;
}

void slab_dump_stats(slab_cache_t *cache) {
    LOG_DEBUG("cache 0x%08" PRIxPTR " (item size %zu) stats:", (uintptr_t)cache,
              cache->item_size);
    LOG_DEBUG("free list count: %zu", list_count(&cache->free));
    LOG_DEBUG("partial list count: %zu", list_count(&cache->partial));
    LOG_DEBUG("full list count: %zu", list_count(&cache->full));
}

static slab_t *prv_slab_get_for_alloc(slab_cache_t *cache) {
    slab_t *slab = NULL;

    if (list_is_empty(&cache->partial)) {
        LOG_FLOW("cache %zu list partial is empty", cache->item_size);
        if (list_is_empty(&cache->free)) {
            LOG_FLOW("cache %zu list free is empty", cache->item_size);
            slab = prv_slab_new(cache, SLAB_MIN_ITEMS_PER_SLAB);
        } else {
            LOG_FLOW("cache %zu list free is not empty", cache->item_size);
            list_node_t *const slab_node = list_pop_first(&cache->free);
            slab = LIST_NODE_TO_STRUCT(slab_node, slab_t, node);
        }
        if (alloc_slab_num_free(&slab->alloc) == 1) {
            list_append(&cache->full, &slab->node);
        } else {
            list_append(&cache->partial, &slab->node);
        }
    } else {
        LOG_FLOW("cache %zu list partial is not empty", cache->item_size);
        list_node_t *const slab_node = cache->partial.p_first_node;
        slab = LIST_NODE_TO_STRUCT(slab_node, slab_t, node);
        if (alloc_slab_num_free(&slab->alloc) == 1) {
            LOG_FLOW("cache %zu slab 0x%08" PRIxPTR " move partial -> full",
                     cache->item_size, (uintptr_t)slab);
            list_pop_first(&cache->partial);
            list_append(&cache->full, slab_node);
        }
    }

    if (!slab) { PANIC("internal error - slab = NULL"); }

    return slab;
}

static slab_t *prv_slab_new(slab_cache_t *cache, size_t min_items) {
    const size_t item_size = cache->item_size;
    const size_t pool_size =
        ((item_size * min_items + sizeof(slab_t)) + (PMM_PAGE_SIZE - 1)) &
        ~(PMM_PAGE_SIZE - 1);
    // Add sizeof(slab_t) because slab_t will be stored at the beginning of the
    // memory range.
    const size_t pool_pages = pool_size / PMM_PAGE_SIZE;

    LOG_FLOW("allocate a new slab for cache %zu", item_size);

    const paddr_t pool_start = pmm_alloc_pages(pool_pages);
    void *const v_pool = (void *)pool_start; // FIXME
    slab_t *const slab = v_pool;

    kmemset(slab, 0, sizeof(*slab));
    slab->cache = cache;

    // NOTE: it is assumed that `item_size` is a power of 2.
    ASSERT((item_size & (item_size - 1)) == 0);
    const size_t metadata_size = sizeof(slab_t);
    const size_t alloc_start =
        pool_start + ((metadata_size + (item_size - 1)) & ~(item_size - 1));
    const size_t lost_size = alloc_start - pool_start;

    alloc_slab_init(&slab->alloc, (void *)alloc_start, pool_size - lost_size,
                    item_size);

    LOG_FLOW("new cache %zu slab 0x%08" PRIxPTR " max items: %zu",
             cache->item_size, (uintptr_t)slab, slab->alloc.num_items);

    // Mark the pages as being owned by the new slab.
    for (paddr_t addr = pool_start;
         addr < pool_start + pool_pages * PMM_PAGE_SIZE;
         addr += PMM_PAGE_SIZE) {
        LOG_FLOW("mark page 0x%08" PRIxPTR
                 " as owned by cache %zu slab 0x%08" PRIxPTR,
                 addr, cache->item_size, (uintptr_t)slab);

        pmm_page_t *const metadata = pmm_paddr_to_page(addr);
        metadata->type = PMM_ALLOC_SLAB;
        metadata->slab = slab;
    }

    return slab;
}
