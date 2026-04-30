#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pmm.h"
#include "types.h"

#define MBI_PMM_MMAP_MAX_ENTRIES 20

#define MBI_FLAG_CMDLINE  (1 << 2)
#define MBI_FLAG_MODS     (1 << 3)
#define MBI_FLAG_MMAP     (1 << 6)
#define MBI_FLAG_FRAMEBUF (1 << 12)

// Value of mbi_t.framebuffer_type for an EGA text mode.
#define MBI_FRAMEBUF_EGA 2

typedef struct [[gnu::packed]] {
    uint32_t flags;

    // flags.0
    uint32_t mem_lower;
    uint32_t mem_upper;

    // flags.1
    uint32_t boot_device;

    // flags.2
    uint32_t cmdline;

    // flags.3
    uint32_t mods_count;
    uint32_t mods_addr;

    // flags.4 or flags.5
    uint32_t syms[4];

    // flags.6
    uint32_t mmap_length;
    uint32_t mmap_addr;

    // flags.7
    uint32_t drives_length;
    uint32_t drives_addr;

    // flags.8
    uint32_t config_table;

    // flags.9
    uint32_t boot_loader_name;

    // flags.10
    uint32_t apm_table;

    // flags.11
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    // flags.12
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} mbi_t;

typedef struct [[gnu::packed]] {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} mbi_mod_t;

typedef enum {
    MBI_MMAP_AVAILABLE = 1,
    MBI_MMAP_ACPI_USABLE = 3,
    MBI_MMAP_RESERVED = 4,
    MBI_MMAP_DEFECTIVE = 5,
} mbi_mmap_type_t;

typedef struct {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} mbi_mmap_entry_t;

void mbi_init(paddr_t mbi_addr);
void mbi_save_on_heap(void);

mbi_t const *mbi_ptr(void);

bool mbi_fill_mmap(const mbi_t *mbi, pmm_mmap_t *mmap);

size_t mbi_num_mods(void);
mbi_mod_t const *mbi_nth_mod(size_t idx);
mbi_mod_t const *mbi_find_mod(char const *p_name);
mbi_mod_t const *mbi_last_mod(void);
