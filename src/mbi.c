#include <alloc.h>
#include <mbi.h>

static mbi_t g_mbi;

void
mbi_copy (mbi_t const * p_src)
{
    // The structure itself.
    //
    __builtin_memcpy(&g_mbi, p_src, sizeof(mbi_t));

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
