#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *heap_alloc(size_t size);
void *heap_alloc_aligned(size_t size, size_t align);
void *heap_realloc(void *ptr, size_t size, size_t align);
void heap_free(void *ptr);

#ifdef __cplusplus
}
#endif
