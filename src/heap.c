#include <ytalloc/ytalloc.h>

#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "heap.h"
#include "kinttypes.h"
#include "kmutex.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"
#include "slab.h"

#define HEAP_MIN_SLAB_ORDER  3  // log2 of 8
#define HEAP_MAX_SLAB_ORDER  11 // log2 of 2048
#define HEAP_NUM_SLAB_ORDERS (HEAP_MAX_SLAB_ORDER - HEAP_MIN_SLAB_ORDER + 1)

#define HEAP_MIN_SLAB_SIZE (1U << HEAP_MIN_SLAB_ORDER)
#define HEAP_MAX_SLAB_SIZE (1U << HEAP_MAX_SLAB_ORDER)

static_assert(HEAP_MAX_SLAB_ORDER >= HEAP_MIN_SLAB_ORDER);

typedef struct {
    task_mutex_t lock;
    size_t nested_lock_cnt;

    slab_cache_t *slab_caches;
    size_t num_slab_caches;
} heap_t;

typedef struct {
    size_t num_pages;
} heap_large_alloc_t;

static heap_t g_heap;
static alloc_static_t g_heap_static;

static void prv_heap_lock(void);
static void prv_heap_unlock(void);

void *heap_get_static_heap(void) {
    return &g_heap_static;
}

void *heap_alloc_static(size_t size) {
    if (g_heap_static.start == 0) {
        LOG_ERROR("static heap is not yet initialized");
        panic("unexpected behavior");
    }

    void *const ptr = alloc_static(&g_heap_static, size);
    if (!ptr) {
        const size_t bytes_used = g_heap_static.next - g_heap_static.start;
        const size_t bytes_total = g_heap_static.end - g_heap_static.start;
        LOG_ERROR("could not statically allocate %zu bytes", size);
        LOG_ERROR("static heap usage: %zu out of %zu bytes", bytes_used,
                  bytes_total);
        panic("out of memory");
    }

    return ptr;
}

void heap_init(void) {
    mutex_init(&g_heap.lock);

    g_heap.num_slab_caches = HEAP_NUM_SLAB_ORDERS;
    g_heap.slab_caches =
        heap_alloc_static(g_heap.num_slab_caches * sizeof(slab_cache_t));

    for (size_t idx = 0; idx < g_heap.num_slab_caches; idx++) {
        const size_t item_size = 1 << (HEAP_MIN_SLAB_ORDER + idx);
        slab_init_cache(&g_heap.slab_caches[idx], item_size);
    }
}

uint32_t heap_end(void) {
    panic("heap_end not implemented");
}

void *heap_alloc(size_t size) {
    return heap_alloc_aligned(size, 1);
}

void *heap_alloc_aligned(size_t size, size_t align) {
    if ((align & (align - 1)) != 0) {
        LOG_ERROR("alignment %zu is not a power of two", align);
        panic("invalid argument");
    }

    prv_heap_lock();

    void *ret_ptr;
    if (size <= HEAP_MAX_SLAB_SIZE) {
        // Items of size `align` will have the alignment of `align` - that's the
        // property of the slab allocator.
        const size_t eff_size = size >= align ? size : align;

        // TODO: calculate the order more efficiently.
        size_t idx;
        for (idx = 0; idx < g_heap.num_slab_caches; idx++) {
            if (g_heap.slab_caches[idx].item_size >= eff_size) { break; }
        }

        ret_ptr = slab_alloc(&g_heap.slab_caches[idx]);
        if (!ret_ptr) {
            LOG_ERROR("failed to allocate %zu bytes aligned at %zu bytes "
                      "(effective size %zu)",
                      size, align, eff_size);
            panic("out of memory");
        }
    } else {
        if (align < PMM_PAGE_SIZE) { align = PMM_PAGE_SIZE; }
        if (align & (PMM_PAGE_SIZE - 1)) {
            LOG_ERROR("pmm: alignment %zu must be a multiple of page size (%u) "
                      "for allocations larger than %u",
                      align, PMM_PAGE_SIZE, HEAP_MAX_SLAB_SIZE);
            panic("invalid argument");
        }

        const size_t aligned_size =
            (size + (PMM_PAGE_SIZE - 1)) & ~(PMM_PAGE_SIZE - 1);
        const size_t num_pages = aligned_size / PMM_PAGE_SIZE;
        const size_t align_pages = align / PMM_PAGE_SIZE;
        const paddr_t addr = pmm_alloc_aligned_pages(num_pages, align_pages);
        // FIXME: physical to virtual?

        heap_large_alloc_t *const large = heap_alloc(sizeof(*large));
        // This static_assert prevents infinite recursion.
        static_assert(sizeof(*large) <= HEAP_MAX_SLAB_SIZE);
        large->num_pages = num_pages;

        for (paddr_t i_addr = addr; i_addr < addr + aligned_size;
             i_addr += PMM_PAGE_SIZE) {
            pmm_page_t *const metadata = pmm_paddr_to_page(i_addr);
            metadata->type = PMM_ALLOC_LARGE;
            metadata->large = large;
            LOG_FLOW("mark page 0x%08" PRIxPTR " as a large allocation %p",
                     i_addr, large);
        }

        ret_ptr = (void *)addr;
    }

    prv_heap_unlock();
    return ret_ptr;
}

void heap_free(void *ptr) {
    if (!ptr) { return; }

    prv_heap_lock();

    const paddr_t addr = (paddr_t)ptr; // FIXME
    pmm_page_t *const metadata = pmm_paddr_to_page(addr);
    heap_large_alloc_t *large;

    switch (metadata->type) {
    case PMM_ALLOC_NONE:
        LOG_FLOW("free %p: PMM_ALLOC_FREE", ptr);
        LOG_ERROR("tried to free a free page 0x%08" PRIxPTR, addr);
        panic("unexpected behavior");

    case PMM_ALLOC_SLAB:
        LOG_FLOW("free %p: PMM_ALLOC_SLAB 0x%08" PRIxPTR, ptr,
                 (uintptr_t)metadata->slab);
        slab_free(metadata->slab, ptr);
        break;

    case PMM_ALLOC_LARGE:
        large = metadata->large;
        LOG_FLOW("free %p: PMM_ALLOC_LARGE 0x%08" PRIxPTR, ptr,
                 (uintptr_t)large);
        LOG_FLOW("large->num_pages = %zu", large->num_pages);
        pmm_free_pages(addr, large->num_pages);
        break;

    default:
        LOG_ERROR("unrecognized value of page 0x%08" PRIxPTR " type (%u)", addr,
                  metadata->type);
        panic("unexpected behavior");
    }

    prv_heap_unlock();
}

void *heap_realloc(void *ptr, size_t size, size_t align) {
    if (!ptr) { return heap_alloc_aligned(size, align); }

    const paddr_t addr = (paddr_t)ptr; // FIXME
    pmm_page_t *const metadata = pmm_paddr_to_page(addr);
    heap_large_alloc_t *large;

    size_t old_size;
    switch (metadata->type) {
    case PMM_ALLOC_NONE:
        LOG_FLOW("realloc: PMM_ALLOC_FREE");
        LOG_ERROR("tried to realloc a free page 0x%08" PRIxPTR, addr);
        panic("unexpected behavior");

    case PMM_ALLOC_SLAB:
        LOG_FLOW("realloc: PMM_ALLOC_SLAB slab = 0x%08" PRIxPTR,
                 (uintptr_t)metadata->slab);
        old_size = slab_item_size(metadata->slab);
        break;

    case PMM_ALLOC_LARGE:
        LOG_FLOW("realloc: PMM_ALLOC_LARGE");
        large = metadata->large;
        old_size = PMM_PAGE_SIZE * large->num_pages;
        break;

    default:
        LOG_ERROR("unrecognized value of page 0x%08" PRIxPTR " type (%u)", addr,
                  metadata->type);
        panic("unexpected behavior");
    }

    const size_t copy_size = size <= old_size ? size : old_size;

    void *const new_ptr = heap_alloc_aligned(size, align);
    kmemcpy(new_ptr, ptr, copy_size);
    heap_free(ptr);
    return new_ptr;
}

static void prv_heap_lock(void) {
    g_heap.nested_lock_cnt++;
    if (!mutex_caller_owns(&g_heap.lock)) { mutex_acquire(&g_heap.lock); }
}

static void prv_heap_unlock(void) {
    if (g_heap.nested_lock_cnt == 0) {
        LOG_ERROR("nested lock counter is zero during unlock");
        panic("unexpected behavior");
    }
    g_heap.nested_lock_cnt--;
    if (g_heap.nested_lock_cnt == 0) { mutex_release(&g_heap.lock); }
}
