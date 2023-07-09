#include <alloc.h>
#include <panic.h>
#include <printf.h>

#include <stdbool.h>

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

static void fill_tag(tag_t * p_tag, bool b_used, size_t size, tag_t * p_next);
static void print_tag(tag_t const * p_tag);

void
alloc_init (void * p_start, size_t num_bytes)
{
    if (!p_start)
    {
        printf("alloc_init: p_start is NULL\n");
        panic("invalid argument");
    }

    if (((uint32_t) p_start) & 0xFFF)
    {
        printf("alloc_init: p_start is not page-aligned\n");
        panic("invalid argument");
    }

    if (0 == num_bytes)
    {
        printf("alloc_init: num_bytes is zero\n");
        panic("invalid argument");
    }

    if (num_bytes % 4096)
    {
        printf("alloc_init: num_bytes must be a multiple of page size\n");
        panic("invalid argument");
    }

    gp_start = ((tag_t *) p_start);
    fill_tag(gp_start, false, (num_bytes - TAG_SIZE), NULL);

    printf("alloc: initialized heap with size of %u bytes starting at %P\n",
           num_bytes, p_start);
}

void *
alloc (size_t num_bytes)
{
    return (alloc_aligned(num_bytes, DEFAULT_ALIGN));
}

void *
alloc_aligned (size_t num_bytes, size_t align)
{
    if (0 == num_bytes)
    {
        printf("alloc: num_bytes is zero\n");
        panic("invalid argument");
    }

    if (0 == align)
    {
        printf("alloc: align is zero\n");
        panic("invalid argument");
    }

    if (NULL == gp_start)
    {
        printf("alloc: heap is not initialized\n");
        panic("unexpected behavior");
    }

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
        printf("alloc: no suitable chunk\n");
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

    return ((void *) (((uint32_t) p_found) + TAG_SIZE + padding));
}

void
alloc_dump_tags (void)
{
    if (NULL == gp_start)
    {
        printf("alloc: no tags\n");
    }

    for (tag_t const * p_tag = gp_start; p_tag != NULL; p_tag = p_tag->p_next)
    {
        print_tag(p_tag);
    }
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
        printf("alloc: print_tag: p_tag is NULL\n");
        panic("invalid argument");
    }

    printf("alloc: tag at %P: ", p_tag);

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
