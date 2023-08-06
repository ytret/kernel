#include <heap.h>
#include <mbi.h>
#include <panic.h>
#include <printf.h>
#include <vmm.h>

#include <stdbool.h>

#define HEAP_SIZE               (4 * 1024 * 1024)

// Default allocation alignment.
//
// NOTE: AHCI code assumes that the alignment is always at least 2 bytes.
//
#define DEFAULT_ALIGN           4

#define TAG_SIZE                (sizeof(tag_t))

#define CHUNK_SIZE_MIN          64
#define CHUNK_SIZE_ALIGN        4

typedef struct tag
{
    bool         b_used;
    size_t       size;
    struct tag * p_next;
} tag_t;

static tag_t * gp_start;

// See link.ld.
//
extern uint32_t ld_vmm_kernel_end;

static uint32_t find_heap_start(void);

static void fill_tag(tag_t * p_tag, bool b_used, size_t size, tag_t * p_next);
static void print_tag(tag_t const * p_tag);
static void check_tags(bool b_after_alloc);

void
heap_init (void)
{
    uint32_t heap_start = find_heap_start();

    gp_start = ((tag_t *) heap_start);
    fill_tag(gp_start, false, (HEAP_SIZE - TAG_SIZE), NULL);

    printf("heap: start at %P, size is %u bytes\n",
           gp_start, HEAP_SIZE);
}

uint32_t
heap_end (void)
{
    return (((uint32_t) gp_start) + HEAP_SIZE);
}

void *
heap_alloc (size_t num_bytes)
{
    return (heap_alloc_aligned(num_bytes, DEFAULT_ALIGN));
}

void *
heap_alloc_aligned (size_t num_bytes, size_t align)
{
    if (0 == num_bytes)
    {
        printf("heap_alloc_aligned: num_bytes is zero\n");
        panic("invalid argument");
    }

    if (0 == align)
    {
        printf("heap_alloc_aligned: align is zero\n");
        panic("invalid argument");
    }

    if (NULL == gp_start)
    {
        printf("heap_alloc_aligned: heap is not initialized\n");
        panic("unexpected behavior");
    }

    // Check the heap before allocation.
    //
    check_tags(false);

    // Make the size aligned.
    //
    num_bytes = (num_bytes + (CHUNK_SIZE_ALIGN - 1)) & ~(CHUNK_SIZE_ALIGN - 1);

    // Traverse the tag list and find the suitable tag.
    //
    tag_t  * p_found = NULL;
    uint32_t chunk_aligned;
    uint32_t padding;
    for (tag_t * p_tag = gp_start; p_tag != NULL; p_tag = p_tag->p_next)
    {
        if (p_tag->b_used)
        {
            continue;
        }

        uint32_t chunk = (((uint32_t) p_tag) + TAG_SIZE);
        chunk_aligned  = ((chunk + (align - 1)) & ~(align - 1));
        padding        = (chunk_aligned - chunk);

        if ((chunk_aligned + num_bytes) <= (chunk + p_tag->size))
        {
            p_found = p_tag;
            break;
        }
    }

    if (!p_found)
    {
        printf("heap_alloc_aligned: no suitable chunk\n");
        panic("allocation failed");
    }

    // Divide the chunk if appropriate.
    //
    if ((p_found->size - num_bytes - padding) > (TAG_SIZE + CHUNK_SIZE_MIN))
    {
        tag_t * p_new_tag =
            ((tag_t *) (((uint32_t) p_found) + TAG_SIZE + padding + num_bytes));

        p_new_tag->b_used = false;
        p_new_tag->size   = (p_found->size - (TAG_SIZE + padding + num_bytes));
        p_new_tag->p_next = p_found->p_next;

        p_found->size   = (padding + num_bytes);
        p_found->p_next = p_new_tag;
    }

    p_found->b_used = true;

    // Check the heap after allocation.
    //
    check_tags(true);

    return ((void *) (((uint32_t) p_found) + TAG_SIZE + padding));
}

void
heap_free (void * p_addr)
{
    (void) p_addr;
}

void
heap_dump_tags (void)
{
    if (NULL == gp_start)
    {
        printf("heap_dump_tags: no tags\n");
    }

    for (tag_t const * p_tag = gp_start; p_tag != NULL; p_tag = p_tag->p_next)
    {
        if (((uint32_t) p_tag) < (((uint32_t) gp_start) + HEAP_SIZE))
        {
            // This tag is within heap.
            //
            print_tag(p_tag);
        }
        else
        {
            return;
        }
    }
}

static uint32_t
find_heap_start (void)
{
    uint32_t last_used_addr;

    mbi_mod_t const * p_last_mod = mbi_last_mod();
    if (!p_last_mod)
    {
        // No modules.
        //
        last_used_addr = (uint32_t) &ld_vmm_kernel_end;
    }
    else
    {
        last_used_addr = p_last_mod->mod_end;
    }

    // Align to the next 4 MiB region.
    //
    uint32_t heap_start =
        (last_used_addr + ((4 * 1024 * 1024) - 1)) & ~((4 * 1024 * 1024) - 1);
    return (heap_start);
}

static void
fill_tag (tag_t * p_tag, bool b_used, size_t size, tag_t * p_next)
{
    if (!p_tag)
    {
        printf("fill_tag: p_tag is NULL\n");
        panic("invalid argument");
    }

    p_tag->size   = size;
    p_tag->b_used = b_used;
    p_tag->p_next = p_next;
}

static void
print_tag (tag_t const * p_tag)
{
    if (NULL == p_tag)
    {
        printf("heap: print_tag: p_tag is NULL\n");
        panic("invalid argument");
    }

    printf("heap: tag at %P: ", p_tag);

    if (p_tag->b_used)
    {
        printf("used, ");
    }
    else
    {
        printf("free, ");
    }

    printf("size = %u bytes\n", p_tag->size);
}

static void
check_tags (bool b_after_alloc)
{
    bool b_panic = false;

    for (tag_t const * p_tag = gp_start; p_tag != NULL; p_tag = p_tag->p_next)
    {
        if (((uint32_t) p_tag) < ((uint32_t) gp_start))
        {
            printf("heap: check_tags: tag %P is below heap\n", p_tag);
            b_panic = true;
            break;
        }

        if (((uint32_t) p_tag) >= (((uint32_t) gp_start) + HEAP_SIZE))
        {
            printf("heap: check_tags: tag %P is above heap\n", p_tag);
            b_panic = true;
            break;
        }

        if ((((uint32_t) p_tag) + sizeof(*p_tag) + p_tag->size)
            > (((uint32_t) gp_start) + HEAP_SIZE))
        {
            printf("heap: check_tags: chunk of tag %P ends beyond heap at"
                   " 0x%08X\n",
                   p_tag, (((uint32_t) p_tag) + sizeof(*p_tag) + p_tag->size));
            b_panic = true;
            break;
        }
    }

    if (b_panic)
    {
        heap_dump_tags();
        panic(b_after_alloc
              ? "invalid heap state after alloc"
              : "invalid heap state before alloc");
    }
}
