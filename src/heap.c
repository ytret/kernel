#include <ytalloc/ytalloc.h>

#include "heap.h"
#include "kmutex.h"
#include "kprintf.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"
#include "slab.h"

#if 0
#define debugf(...) kprintf(__VA_ARGS__);
#else
#define debugf(...)                                                            \
    do {                                                                       \
    } while (0);
#endif

#define HEAP_MIN_SLAB_ORDER  3  // log2 of 8
#define HEAP_MAX_SLAB_ORDER  11 // log2 of 2048
#define HEAP_NUM_SLAB_ORDERS (HEAP_MAX_SLAB_ORDER - HEAP_MIN_SLAB_ORDER + 1)

#define HEAP_MIN_SLAB_SIZE (1 << HEAP_MIN_SLAB_ORDER)
#define HEAP_MAX_SLAB_SIZE (1 << HEAP_MAX_SLAB_ORDER)

static_assert(HEAP_MAX_SLAB_ORDER >= HEAP_MIN_SLAB_ORDER);

typedef struct {
    task_mutex_t lock;
    slab_cache_t *slab_caches;
    size_t num_slab_caches;
} heap_t;

typedef struct {
    size_t num_pages;
} heap_large_alloc_t;

static heap_t g_heap;
static alloc_static_t g_heap_static;

void *heap_get_static_heap(void) {
    return &g_heap_static;
}

void *heap_alloc_static(size_t size) {
    if (g_heap_static.start == 0) {
        kprintf("heap: static heap is not yet initialized\n");
        panic("unexpected behavior");
    }

    void *const ptr = alloc_static(&g_heap_static, size);
    if (!ptr) {
        const size_t bytes_used = g_heap_static.next - g_heap_static.start;
        const size_t bytes_total = g_heap_static.end - g_heap_static.start;
        kprintf("heap: could not statically allocate %u bytes\n", size);
        kprintf("heap: static heap usage: %u out of %u bytes\n", bytes_used,
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
        kprintf("heap: alignment %u is not a power of two\n", align);
        panic("invalid argument");
    }

    mutex_acquire(&g_heap.lock);

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
            kprintf("heap: failed to allocate %u bytes aligned at %u bytes "
                    "(effective size %u)\n",
                    size, align, eff_size);
            panic("out of memory");
        }
    } else {
        if (align < PMM_PAGE_SIZE) { align = PMM_PAGE_SIZE; }
        if (align & (PMM_PAGE_SIZE - 1)) {
            kprintf("pmm: alignment %u must be a multiple of page size (%u) "
                    "for allocations larger than %u\n",
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
            metadata->type = PMM_PAGE_LARGE;
            metadata->large = large;
            debugf("heap: mark page 0x%08x as a large allocation\n", i_addr);
        }

        ret_ptr = (void *)addr;
    }

    mutex_release(&g_heap.lock);
    return ret_ptr;
}

void heap_free(void *ptr) {
    if (!ptr) { return; }

    mutex_acquire(&g_heap.lock);

    const paddr_t addr = (paddr_t)ptr; // FIXME
    pmm_page_t *const metadata = pmm_paddr_to_page(addr);
    heap_large_alloc_t *large;

    switch (metadata->type) {
    case PMM_PAGE_FREE:
        debugf("heap: free: PMM_PAGE_FREE\n");
        kprintf("heap: tried to free a free page 0x%08x\n", addr);
        panic("unexpected behavior");

    case PMM_PAGE_SLAB:
        debugf("heap: free: PMM_PAGE_SLAB\n");
        debugf("heap: slab = 0x%08x\n", metadata->slab);
        slab_free(metadata->slab, ptr);
        break;

    case PMM_PAGE_LARGE:
        debugf("heap: free: PMM_PAGE_LARGE\n");
        large = metadata->large;
        debugf("heap: large->num_pages = %u\n", large->num_pages);
        pmm_free_pages(addr, large->num_pages);
        break;

    default:
        kprintf("heap: unrecognized value of page 0x%08x type (%u)\n", addr,
                metadata->type);
        panic("unexpected behavior");
    }

    mutex_release(&g_heap.lock);
}

void *heap_realloc(void *ptr, size_t size, size_t align) {
    if (!ptr) { return heap_alloc_aligned(size, align); }

    const paddr_t addr = (paddr_t)ptr; // FIXME
    pmm_page_t *const metadata = pmm_paddr_to_page(addr);
    heap_large_alloc_t *large;

    size_t old_size;
    switch (metadata->type) {
    case PMM_PAGE_FREE:
        debugf("heap: realloc: PMM_PAGE_FREE\n");
        kprintf("heap: tried to realloc a free page 0x%08x\n", addr);
        panic("unexpected behavior");

    case PMM_PAGE_SLAB:
        debugf("heap: realloc: PMM_PAGE_SLAB\n");
        debugf("heap: slab = 0x%08x\n", metadata->slab);
        old_size = slab_item_size(metadata->slab);
        break;

    case PMM_PAGE_LARGE:
        debugf("heap: realloc: PMM_PAGE_LARGE\n");
        large = metadata->large;
        old_size = PMM_PAGE_SIZE * large->num_pages;
        break;

    default:
        kprintf("heap: unrecognized value of page 0x%08x type (%u)\n", addr,
                metadata->type);
        panic("unexpected behavior");
    }

    const size_t copy_size = size <= old_size ? size : old_size;

    void *const new_ptr = heap_alloc_aligned(size, align);
    kmemcpy(new_ptr, ptr, copy_size);
    heap_free(ptr);
    return new_ptr;
}
