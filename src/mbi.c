#include <alloc.h>
#include <mbi.h>
#include <string.h>

static mbi_t g_mbi;

void
mbi_copy (mbi_t const * p_src)
{
    // The structure itself.
    //
    __builtin_memcpy(&g_mbi, p_src, sizeof(mbi_t));

    if (p_src->flags & MBI_FLAG_MODS)
    {
        // Copy the modules info.
        //
        uint8_t * p_mods = alloc(p_src->mods_count * sizeof(mbi_mod_t));
        __builtin_memcpy(p_mods,
                         ((void const *) p_src->mods_addr),
                         (p_src->mods_count * sizeof(mbi_mod_t)));
        g_mbi.mods_addr = ((uint32_t) p_mods);
    }

    if (p_src->flags & MBI_FLAG_MMAP)
    {
        // Copy the memory map.
        //
        uint8_t * p_mmap = alloc(p_src->mmap_length);
        __builtin_memcpy(p_mmap,
                         ((void const *) p_src->mmap_addr),
                         p_src->mmap_length);
        g_mbi.mmap_addr = ((uint32_t) p_mmap);
    }
}

mbi_t const *
mbi_get_ptr (void)
{
    return (&g_mbi);
}

void const *
mbi_find_mod (mbi_t const * p_mbi, char const * p_name)
{
    if (!(p_mbi->flags & MBI_FLAG_MODS))
    {
        return (NULL);
    }

    for (size_t idx = 0; idx < p_mbi->mods_count; idx++)
    {
        mbi_mod_t const * p_mods = ((mbi_mod_t const *) p_mbi->mods_addr);

        if (!p_mods[idx].string)
        {
            continue;
        }

        if (string_equals(((char const *) p_mods[idx].string), p_name))
        {
            return ((void const *) p_mods[idx].mod_start);
        }
    }

    return (NULL);
}
