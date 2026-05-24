#pragma once

#include "types.h"

typedef struct {
    paddr_t phys_start;
    paddr_t phys_end;
    const char *name;
} arch_boot_mod_t;

typedef struct {
    paddr_t addr;
    uint32_t height;
    uint32_t width;
    uint32_t pitch;
    uint8_t bpp;
} arch_boot_fb_t;

void arch_boot_init(void);

size_t arch_boot_num_mods(void);
const arch_boot_mod_t *arch_boot_find_mod(const char *name);
const arch_boot_mod_t *arch_boot_nth_mod(size_t idx);

const arch_boot_fb_t *arch_boot_framebuf(void);
