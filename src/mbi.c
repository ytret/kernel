#include <alloc.h>
#include <mbi.h>
#include <string.h>

#include <panic.h>
#include <printf.h>

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

size_t
mbi_num_mods (mbi_t const * p_mbi)
{
    if (p_mbi->flags & MBI_FLAG_MODS)
    {
        return (p_mbi->mods_count);
    }
    else
    {
        return (0);
    }
}

mbi_mod_t const *
mbi_nth_mod (mbi_t const * p_mbi, size_t idx)
{
    if (idx >= mbi_num_mods(p_mbi))
    {
        return (NULL);
    }

    mbi_mod_t const * p_mods = (mbi_mod_t const *) p_mbi->mods_addr;
    return (&p_mods[idx]);
}

mbi_mod_t const *
mbi_find_mod (mbi_t const * p_mbi, char const * p_name)
{
    for (size_t idx = 0; idx < mbi_num_mods(p_mbi); idx++)
    {
        mbi_mod_t const * p_mod = mbi_nth_mod(p_mbi, idx);

        if (!p_mod)
        {
            printf("mbi_nth_mod() returned NULL for index %u < number of"
                   " modules %u\n", idx, mbi_num_mods(p_mbi));
            panic("unexpected behavior");
        }

        if (string_equals((char const *) p_mod->string, p_name))
        {
            return (p_mod);
        }
    }

    return (NULL);
}

mbi_mod_t const *
mbi_last_mod (mbi_t const * p_mbi)
{
    if (0 == mbi_num_mods(p_mbi))
    {
        return (NULL);
    }
    else
    {
        return (mbi_nth_mod(p_mbi, (mbi_num_mods(p_mbi) - 1)));
    }
}
