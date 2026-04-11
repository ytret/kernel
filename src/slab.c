#include <ytalloc/ytalloc.h>

#include "assert.h"
#include "kprintf.h"
#include "memfun.h"
#include "pmm.h"
#include "slab.h"

#define SLAB_MIN_ITEMS_PER_SLAB 2

#if 0
#define debugf(...) kprintf(__VA_ARGS__);
#else
#define debugf(...)                                                            \
    do {                                                                       \
    } while (0);
#endif

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
    if (!v_slab) {
        kprintf("slab: v_slab = NULL\n");
        panic("invalid argument");
    }
    if (!ptr) { return; }

    debugf("slab: free item at 0x%08x of slab 0x%08x\n", (uintptr_t)ptr,
           (uintptr_t)v_slab);

    slab_t *const slab = v_slab;
    alloc_slab_free(&slab->alloc, ptr);

    slab_cache_t *const cache = slab->cache;
    const size_t used_items = alloc_slab_num_used(&slab->alloc);
    if (used_items == slab->alloc.num_items - 1) {
        // full -> partial
        debugf("slab: cache %u slab 0x%08x move full -> partial\n",
               cache->item_size, (uintptr_t)slab);
        list_remove(&cache->full, &slab->node);
        list_append(&cache->partial, &slab->node);
    } else if (used_items == 0) {
        // partial -> free
        debugf("slab: cache %u slab 0x%08x move partial -> free\n",
               cache->item_size, (uintptr_t)slab);
        list_remove(&cache->partial, &slab->node);
        list_append(&cache->free, &slab->node);
    }
}

size_t slab_item_size(const void *v_slab) {
    if (!v_slab) {
        kprintf("slab: v_slab = NULL\n");
        panic("invalid argument");
    }

    const slab_t *const slab = v_slab;
    if (!slab->cache) {
        kprintf("slab: cache is NULL\n");
        panic("unexpected behavior");
    }

    return slab->cache->item_size;
}

void slab_print_stats(slab_cache_t *cache) {
    kprintf("slab: cache 0x%08x (item size %u) stats:\n", (uintptr_t)cache,
            cache->item_size);
    kprintf("slab: free list count: %u\n", list_count(&cache->free));
    kprintf("slab: partial list count: %u\n", list_count(&cache->partial));
    kprintf("slab: full list count: %u\n", list_count(&cache->full));
}

static slab_t *prv_slab_get_for_alloc(slab_cache_t *cache) {
    slab_t *slab = NULL;

    if (list_is_empty(&cache->partial)) {
        debugf("slab: cache %u list partial is empty\n", cache->item_size);
        if (list_is_empty(&cache->free)) {
            debugf("slab: cache %u list free is empty\n", cache->item_size);
            slab = prv_slab_new(cache, SLAB_MIN_ITEMS_PER_SLAB);
        } else {
            debugf("slab: cache %u list free is not empty\n", cache->item_size);
            list_node_t *const slab_node = list_pop_first(&cache->free);
            slab = LIST_NODE_TO_STRUCT(slab_node, slab_t, node);
        }
        if (alloc_slab_num_free(&slab->alloc) == 1) {
            list_append(&cache->full, &slab->node);
        } else {
            list_append(&cache->partial, &slab->node);
        }
    } else {
        debugf("slab: cache %u list partial is not empty\n", cache->item_size);
        list_node_t *const slab_node = cache->partial.p_first_node;
        slab = LIST_NODE_TO_STRUCT(slab_node, slab_t, node);
        if (alloc_slab_num_free(&slab->alloc) == 1) {
            debugf("slab: cache %u slab 0x%08x move partial -> full\n",
                   cache->item_size, (uintptr_t)slab);
            list_pop_first(&cache->partial);
            list_append(&cache->full, slab_node);
        }
    }

    if (!slab) {
        kprintf("slab: internal error: slab = NULL\n");
        panic("unexpected behavior");
    }

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

    debugf("slab: allocate a new slab for cache %u\n", item_size);

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

    debugf("slab: new cache %u slab 0x%08x max items: %u\n", cache->item_size,
           (uintptr_t)slab, slab->alloc.num_items);

    // Mark the pages as being owned by the new slab.
    for (paddr_t addr = pool_start;
         addr < pool_start + pool_pages * PMM_PAGE_SIZE;
         addr += PMM_PAGE_SIZE) {
        debugf("slab: mark page 0x%08x as owned by cache %u slab 0x%08x\n",
               addr, cache->item_size, (uintptr_t)slab);

        pmm_page_t *const metadata = pmm_paddr_to_page(addr);
        metadata->type = PMM_ALLOC_SLAB;
        metadata->slab = slab;
    }

    return slab;
}
