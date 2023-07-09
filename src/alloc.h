#pragma once

#include <stddef.h>
#include <stdint.h>

void   alloc_init(void * p_start, size_t num_bytes);
void * alloc(size_t num_bytes);
void * alloc_aligned(size_t num_bytes, size_t align);

void alloc_dump_tags(void);
