#include "heap.h"
#include "kstring.h"
#include "log.h"
#include "mbi.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"

static mbi_t *gp_mbi;
static pmm_region_t g_mbi_pmm_regions[MBI_PMM_MMAP_MAX_ENTRIES];

/*
 * Sets the internal MBI struct pointer for the mbi_* functions to use.
 *
 * NOTE: lifetimes of the MBI struct and data it points to must span until
 * mbi_deep_copy() is called, which copies them to heap.  After that, the
 * original data may be overwritten.
 */
void mbi_init(uint32_t mbi_addr) {
    gp_mbi = (mbi_t *)mbi_addr;
}

/*
 * Makes a deep copy of the MBI on heap.
 *
 * NOTE: requires the heap to be initialized.
 */
void mbi_save_on_heap(void) {
    // The structure itself.
    mbi_t const *p_src = gp_mbi;
    gp_mbi = heap_alloc(sizeof(mbi_t));
    __builtin_memcpy(gp_mbi, p_src, sizeof(mbi_t));

    if (p_src->flags & MBI_FLAG_CMDLINE) {
        // Copy the cmdline.
        const size_t cmdline_len = string_len((const char *)p_src->cmdline);
        char *const cmdline = heap_alloc(cmdline_len + 1);
        kmemcpy(cmdline, (void *)p_src->cmdline, cmdline_len + 1);
        gp_mbi->cmdline = (uint32_t)cmdline;
    }
    if (p_src->flags & MBI_FLAG_MODS) {
        // Copy the modules info.
        uint8_t *p_mods = heap_alloc(p_src->mods_count * sizeof(mbi_mod_t));
        __builtin_memcpy(p_mods, ((void const *)p_src->mods_addr),
                         (p_src->mods_count * sizeof(mbi_mod_t)));
        gp_mbi->mods_addr = ((uint32_t)p_mods);
    }

    if (p_src->flags & MBI_FLAG_MMAP) {
        // Copy the memory map.
        uint8_t *p_mmap = heap_alloc(p_src->mmap_length);
        __builtin_memcpy(p_mmap, ((void const *)p_src->mmap_addr),
                         p_src->mmap_length);
        gp_mbi->mmap_addr = ((uint32_t)p_mmap);
    }
}

mbi_t const *mbi_ptr(void) {
    return gp_mbi;
}

bool mbi_fill_mmap(const mbi_t *mbi, pmm_mmap_t *mmap) {
    if (!(mbi->flags & MBI_FLAG_MMAP)) { return false; }

    const uint32_t map_addr = mbi->mmap_addr;
    const uint32_t map_len = mbi->mmap_length;

    uint32_t offset = 0;
    size_t pmm_idx = 0;
    while (offset < map_len && pmm_idx < MBI_PMM_MMAP_MAX_ENTRIES) {
        const mbi_mmap_entry_t *const mbi_entry =
            (const mbi_mmap_entry_t *)(map_addr + offset);
        pmm_region_t *const pmm_region = &g_mbi_pmm_regions[pmm_idx];

        pmm_region_type_t pmm_type;
        switch (mbi_entry->type) {
        case MBI_MMAP_AVAILABLE:
            pmm_type = PMM_REGION_AVAILABLE;
            break;
        default:
            pmm_type = PMM_REGION_RESERVED;
            break;
        }

        pmm_region->type = pmm_type;
        pmm_region->start = mbi_entry->base_addr;
        pmm_region->end_incl = (mbi_entry->base_addr - 1) + mbi_entry->length;

        list_append(&mmap->entry_list, &g_mbi_pmm_regions[pmm_idx].node);

        offset += 4 + mbi_entry->size;
        pmm_idx++;
    }

    return true;
}

size_t mbi_num_mods(void) {
    if (gp_mbi->flags & MBI_FLAG_MODS) {
        return gp_mbi->mods_count;
    } else {
        return 0;
    }
}

mbi_mod_t const *mbi_nth_mod(size_t idx) {
    if (idx >= mbi_num_mods()) { return NULL; }

    mbi_mod_t const *p_mods = (mbi_mod_t const *)gp_mbi->mods_addr;
    return &p_mods[idx];
}

mbi_mod_t const *mbi_find_mod(char const *p_name) {
    for (size_t idx = 0; idx < mbi_num_mods(); idx++) {
        mbi_mod_t const *p_mod = mbi_nth_mod(idx);

        if (!p_mod) {
            panic_enter();
            LOG_ERROR("mbi_nth_mod() returned NULL for index %u < number of "
                      "modules %u",
                      idx, mbi_num_mods());
            panic("unexpected behavior");
        }

        if (string_equals((char const *)p_mod->string, p_name)) {
            return p_mod;
        }
    }

    return NULL;
}

mbi_mod_t const *mbi_last_mod(void) {
    if (0 == mbi_num_mods()) {
        return NULL;
    } else {
        return mbi_nth_mod(mbi_num_mods() - 1);
    }
}
