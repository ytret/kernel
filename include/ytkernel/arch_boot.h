#pragma once

#include "types.h"

typedef struct {
    paddr_t phys_start;
    paddr_t phys_end;
    const char *name;
} arch_boot_mod_t;

void arch_boot_init(void);
size_t arch_boot_num_mods(void);
const arch_boot_mod_t *arch_boot_find_mod(const char *name);
const arch_boot_mod_t *arch_boot_nth_mod(size_t idx);
