#include "heap.h"
#include "kinttypes.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "panic.h"
#include "pmm.h"

#include "arch/x86/mbi.h"

static uint32_t g_mbi_magic_num;
static mbi_t *gp_mbi;
static pmm_region_t g_mbi_pmm_regions[MBI_PMM_MMAP_MAX_ENTRIES];

/**
 * Sets an internal MBI struct pointer that mbi_* functions use.
 *
 * @warning
 * Lifetimes of the MBI struct and the data it points to must span until
 * #mbi_save_on_heap() is called.  After that, the original data may be
 * discarded.
 */
void mbi_init(uint32_t magic_num, paddr_t mbi_addr) {
    if (mbi_addr > VADDR_MAX) {
        PANIC("MBI structure is located beyond 4 GiB");
    }

    g_mbi_magic_num = magic_num;
    gp_mbi = (mbi_t *)(vaddr_t)mbi_addr;
}

void mbi_check_magic(void) {
    if (g_mbi_magic_num == MULTIBOOT_MAGIC_NUM) {
        LOG_INFO("booted by a multiboot-compliant bootloader");
        LOG_DEBUG("multiboot information structure is at %p", gp_mbi);
    } else {
        LOG_ERROR("main: magic number: 0x%" PRIx32 ", expected: 0x%x",
                  g_mbi_magic_num, MULTIBOOT_MAGIC_NUM);
        PANIC("booted by an unknown bootloader");
    }
}

/**
 * Makes a deep copy of the MBI struct on heap.
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

        if (cmdline_len > 0) {
            LOG_DEBUG("mbi cmdline (len %zu): %s", cmdline_len,
                      (const char *)gp_mbi->cmdline);
        } else {
            LOG_DEBUG("mbi cmdline (len %zu) is empty", cmdline_len);
        }
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
        case MBI_MMAP_AVAILABLE: pmm_type = PMM_REGION_AVAILABLE; break;
        default:                 pmm_type = PMM_REGION_RESERVED; break;
        }

        pmm_region->type = pmm_type;
        pmm_region->start = mbi_entry->base_addr;
        pmm_region->end_incl = (mbi_entry->base_addr - 1) + mbi_entry->length;

        LOG_DEBUG("added region physical 0x%016llx..0x%016llx type %d",
                  pmm_region->start, pmm_region->end_incl, pmm_region->type);

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
    LOG_FLOW("find module named '%s'", p_name);

    for (size_t idx = 0; idx < mbi_num_mods(); idx++) {
        mbi_mod_t const *p_mod = mbi_nth_mod(idx);

        if (!p_mod) {
            PANIC("mbi_nth_mod() returned NULL for index %zu < number of "
                  "modules %zu",
                  idx, mbi_num_mods());
        }

        LOG_FLOW("module idx %zu string '%s'", idx,
                 (const char *)p_mod->string);

        if (string_equals((const char *)p_mod->string, p_name)) {
            LOG_FLOW("exact match, return %p", p_mod);
            return p_mod;
        }
    }

    LOG_FLOW("no match, return %p", NULL);
    return NULL;
}

mbi_mod_t const *mbi_last_mod(void) {
    if (0 == mbi_num_mods()) {
        return NULL;
    } else {
        return mbi_nth_mod(mbi_num_mods() - 1);
    }
}
