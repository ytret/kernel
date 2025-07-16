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
    free(p_addr);
}
