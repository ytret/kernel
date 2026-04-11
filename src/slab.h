#pragma once

#include "list.h"

typedef struct {
    size_t item_size;
    list_t free;    //!< List of slabs with no used items.
    list_t partial; //!< List of slabs with some, but not all items used.
    list_t full;    //!< List of slabs with all items used.
} slab_cache_t;

void slab_init_cache(slab_cache_t *cache, size_t item_size);
void *slab_alloc(slab_cache_t *cache);
void slab_free(void *v_slab, void *ptr);
size_t slab_item_size(const void *v_slab);

void slab_print_stats(slab_cache_t *cache);
