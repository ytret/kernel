#pragma once

#include <stddef.h>
#include <stdint.h>

void heap_init(uint32_t start);
uint32_t heap_end(void);

void *heap_alloc(size_t num_bytes);
void *heap_alloc_aligned(size_t num_bytes, size_t align);
void *heap_realloc(void *p_addr, size_t num_bytes, size_t align);
void heap_free(void *p_addr);

void heap_dump_tags(void);
