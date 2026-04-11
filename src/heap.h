#pragma once

#include <stddef.h>
#include <stdint.h>

void *heap_get_static_heap(void);
void *heap_alloc_static(size_t size);

void heap2_init(void);
void *heap2_alloc(size_t size);
void *heap2_alloc_aligned(size_t size, size_t align);
void heap2_free(void *ptr);

void heap_init(uint32_t start);
uint32_t heap_end(void);

void *heap_alloc(size_t num_bytes);
void *heap_alloc_aligned(size_t num_bytes, size_t align);
void *heap_realloc(void *p_addr, size_t num_bytes, size_t align);
void heap_free(void *p_addr);

void heap_dump_tags(void);
