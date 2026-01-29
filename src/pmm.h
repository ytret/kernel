#pragma once

#include <stddef.h>
#include <stdint.h>

#define PMM_MMAP_MAX_ENTRIES 20

typedef enum {
    PMM_REGION_AVAILABLE,
    PMM_REGION_RESERVED,
} pmm_region_type_t;

typedef struct {
    pmm_region_type_t type;
    uint64_t start;
    uint64_t end_incl;
} pmm_region_t;

typedef struct {
    size_t num_entries;
    pmm_region_t entries[PMM_MMAP_MAX_ENTRIES];
} pmm_mmap_t;

void pmm_init(const pmm_mmap_t *mmap);

void pmm_push_page(uint32_t addr);
uint32_t pmm_pop_page(void);
