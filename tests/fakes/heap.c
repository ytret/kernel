#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "heap.h"
#include "memfun.h"

void *heap_alloc(size_t size) {
    void *const ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "fake %s: failed to allocate %zu bytes\n", __func__,
                size);
        abort();
    }
    return ptr;
}

void *heap_alloc_aligned(size_t size, size_t align) {
    if (align == 0) {
        fprintf(stderr, "%s: align = 0\n", __func__);
        abort();
    }
    if ((align & (align - 1)) != 0) {
        fprintf(stderr, "%s: align (%zu) must be power of two\n", __func__,
                align);
        abort();
    }

    // Precondition `size` and `align` for aligned_alloc().
    if (align < sizeof(void *)) { align = sizeof(void *); }
    size = (size + align - 1) & ~(align - 1);

    void *const ptr = aligned_alloc(align, size);
    if (!ptr) {
        fprintf(stderr,
                "fake %s: failed to allocate %zu bytes with align %zu\n",
                __func__, size, align);
        abort();
    }
    return ptr;
}

void *heap_realloc(void *ptr, size_t size, size_t align) {
    size_t old_size;
#ifdef __APPLE__
    old_size = malloc_size(ptr);
#else
    old_size = malloc_usable_size(ptr);
#endif

    const size_t copy_size = size >= old_size ? old_size : size;

    void *const new_ptr = heap_alloc_aligned(size, align);
    if (!new_ptr) {
        fprintf(stderr, "fake %s: new alloc of size %zu align %zu failed\n",
                __func__, size, align);
        abort();
    }

    kmemcpy(new_ptr, ptr, copy_size);

    heap_free(ptr);
    return new_ptr;
}

void heap_free(void *ptr) {
    if (!ptr) {
        fprintf(stderr, "fake %s: ptr = NULL\n", __func__);
        abort();
    }
    free(ptr);
}
