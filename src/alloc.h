#pragma once

#include <mbi.h>

#include <stddef.h>
#include <stdint.h>

void     alloc_init(mbi_t const * p_mbi);
uint32_t alloc_end(void);

void * alloc(size_t num_bytes);
void * alloc_aligned(size_t num_bytes, size_t align);

void alloc_free(void * p_addr);

void alloc_dump_tags(void);
