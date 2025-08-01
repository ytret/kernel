#include <stdio.h>
#include <stdlib.h>

#include "heap.h"

void *heap_alloc(size_t num_bytes) {
    void *const ptr = malloc(num_bytes);
    if (!ptr) {
        fprintf(stderr, "fake heap_alloc: failed to allocate %zu bytes\n",
                num_bytes);
        abort();
    }
    return ptr;
}

void heap_free(void *p_addr) {
    if (!p_addr) {
        fprintf(stderr, "fake heap_free: p_addr = NULL\n");
        abort();
    }
    free(p_addr);
}
