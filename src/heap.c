#include <stdbool.h>

#include "heap.h"
#include "kprintf.h"
#include "mbi.h"
#include "mutex.h"
#include "panic.h"

#define HEAP_SIZE (12 * 1024 * 1024)

#define MIN_ALIGN 4

#define TAG_SIZE (sizeof(tag_t))

#define CHUNK_SIZE_MIN   64
#define CHUNK_SIZE_ALIGN 4

#define PADDING_BYTE 0xFF
#define PADDING_DWORD                                                          \
    ((uint32_t)((PADDING_BYTE << 24) | (PADDING_BYTE << 16) |                  \
                (PADDING_BYTE << 8) | PADDING_BYTE))
static_assert(PADDING_BYTE != 0x00, "padding byte must not be a zero");

/*
 * Both MIN_ALIGN and CHUNK_SIZE_ALIGN must be powers of two and not less than
 * 4, so that each tag address is aligned at 4 bytes, too.
 */
static_assert((MIN_ALIGN & (MIN_ALIGN - 1)) == 0,
              "MIN_ALIGN must be a power of two");
static_assert((CHUNK_SIZE_ALIGN & (CHUNK_SIZE_ALIGN - 1)) == 0,
              "CHUNK_SIZE_ALIGN must be a power of two");
static_assert(MIN_ALIGN >= 4, "MIN_ALIGN must be more than 4");
static_assert(CHUNK_SIZE_ALIGN >= 4, "CHUNK_SIZE_ALIGN must be more than 4");

typedef struct tag {
    bool b_used;
    size_t size;
    struct tag *p_next;
} tag_t;

static tag_t *gp_heap_start;
static task_mutex_t g_heap_mutex;

// See link.ld.
extern uint32_t ld_vmm_kernel_end;

static uint32_t find_heap_start(void);

static void print_tag(tag_t const *p_tag);
static void check_tags(void);

void heap_init(void) {
    uint32_t heap_start = find_heap_start();

    gp_heap_start = ((tag_t *)heap_start);
    __builtin_memset(gp_heap_start, 0, TAG_SIZE);
    gp_heap_start->b_used = false;
    gp_heap_start->size = HEAP_SIZE - TAG_SIZE;
    gp_heap_start->p_next = NULL;

    kprintf("heap: start at %P, size is %u bytes\n", gp_heap_start, HEAP_SIZE);
}

uint32_t heap_end(void) {
    return (uint32_t)gp_heap_start + HEAP_SIZE;
}

void *heap_alloc(size_t num_bytes) {
    return heap_alloc_aligned(num_bytes, MIN_ALIGN);
}

void *heap_alloc_aligned(size_t num_bytes, size_t align) {
    mutex_acquire(&g_heap_mutex);

    if (num_bytes == 0) {
        panic_enter();
        kprintf("heap_alloc_aligned: num_bytes is zero\n");
        panic("invalid argument");
    }

    if (align < MIN_ALIGN) {
        align = MIN_ALIGN;
    } else if ((align & (align - 1)) != 0) {
        panic_enter();
        kprintf("heap_alloc_aligned: align must be a power of two\n");
        panic("invalid argument");
    }

    if (!gp_heap_start) {
        panic_enter();
        kprintf("heap_alloc_aligned: heap is not initialized\n");
        panic("unexpected behavior");
    }

    check_tags();

    // Make the size aligned.
    num_bytes = (num_bytes + (CHUNK_SIZE_ALIGN - 1)) & ~(CHUNK_SIZE_ALIGN - 1);

    // Traverse the tag list and find a suitable chunk.
    tag_t *p_found = NULL;
    uint32_t chunk_aligned;
    uint32_t num_padding = 0;
    for (tag_t *p_tag = gp_heap_start; p_tag != NULL; p_tag = p_tag->p_next) {
        if (p_tag->b_used) { continue; }

        uint32_t chunk = (uint32_t)p_tag + TAG_SIZE;
        chunk_aligned = (chunk + (align - 1)) & ~(align - 1);
        num_padding = chunk_aligned - chunk;

        if ((chunk_aligned + num_bytes) <= (chunk + p_tag->size)) {
            p_found = p_tag;
            break;
        }
    }

    if (!p_found) {
        panic_enter();
        kprintf("heap_alloc_aligned: no suitable chunk\n");
        panic("allocation failed");
    }

    __builtin_memset((void *)((uint32_t)p_found + TAG_SIZE), PADDING_BYTE,
                     num_padding);

    // Divide the chunk if appropriate.
    if (p_found->size - num_bytes - num_padding > TAG_SIZE + CHUNK_SIZE_MIN) {
        tag_t *p_new_tag =
            (tag_t *)((uint32_t)p_found + TAG_SIZE + num_padding + num_bytes);

        p_new_tag->b_used = false;
        p_new_tag->size = p_found->size - (num_padding + num_bytes + TAG_SIZE);
        p_new_tag->p_next = p_found->p_next;

        p_found->size = num_padding + num_bytes;
        p_found->p_next = p_new_tag;
    }

    p_found->b_used = true;

    check_tags();

    mutex_release(&g_heap_mutex);
    return (void *)((uint32_t)p_found + TAG_SIZE + num_padding);
}

void heap_free(void *p_addr) {
    mutex_acquire(&g_heap_mutex);

    tag_t *p_tag = (tag_t *)((uint32_t)p_addr - TAG_SIZE);
    while ((uint32_t)p_tag->p_next == PADDING_DWORD) {
        p_tag = (tag_t *)((uint32_t)p_tag - 4);
    }
    p_tag->b_used = false;
    check_tags();

    mutex_release(&g_heap_mutex);
}

void heap_dump_tags(void) {
    mutex_acquire(&g_heap_mutex);

    if (NULL == gp_heap_start) { kprintf("heap_dump_tags: no tags\n"); }

    for (tag_t const *p_tag = gp_heap_start; p_tag != NULL;
         p_tag = p_tag->p_next) {
        if (p_tag >= gp_heap_start &&
            ((uint32_t)p_tag < (uint32_t)gp_heap_start + HEAP_SIZE)) {
            print_tag(p_tag);
        } else {
            // This tag is outside the heap and invalid. Do not print its
            // "fields" because the memory may be inaccessible.
            mutex_release(&g_heap_mutex);
            return;
        }
    }

    mutex_release(&g_heap_mutex);
}

static uint32_t find_heap_start(void) {
    uint32_t last_used_addr;

    mbi_mod_t const *p_last_mod = mbi_last_mod();
    if (p_last_mod) {
        last_used_addr = p_last_mod->mod_end;
    } else {
        last_used_addr = (uint32_t)&ld_vmm_kernel_end;
    }

    // Align to the next 4 MiB region.
    uint32_t heap_start =
        (last_used_addr + ((4 * 1024 * 1024) - 1)) & ~((4 * 1024 * 1024) - 1);
    return heap_start;
}

static void print_tag(tag_t const *p_tag) {
    if (!p_tag) {
        panic_enter();
        kprintf("heap: print_tag: p_tag is NULL\n");
        panic("invalid argument");
    }

    kprintf("heap: tag at %P: ", p_tag);

    if (p_tag->b_used) {
        kprintf("used, ");
    } else {
        kprintf("free, ");
    }

    kprintf("size = %u bytes\n", p_tag->size);
}

static void check_tags(void) {
    bool b_panic = false;

    tag_t const *p_prev = NULL;
    for (tag_t const *p_tag = gp_heap_start; p_tag != NULL;
         p_prev = p_tag, p_tag = p_tag->p_next) {
        if (p_prev && p_tag <= p_prev) {
            panic_enter();
            kprintf("heap: check_tags: tag %P is below its previous tag %P\n",
                    p_tag, p_prev);
            b_panic = true;
            break;
        }

        if ((uint32_t)p_tag < (uint32_t)gp_heap_start) {
            panic_enter();
            kprintf("heap: check_tags: tag %P is below heap\n", p_tag);
            b_panic = true;
            break;
        }

        if ((uint32_t)p_tag >= (uint32_t)gp_heap_start + HEAP_SIZE) {
            panic_enter();
            kprintf("heap: check_tags: tag %P is above heap\n", p_tag);
            b_panic = true;
            break;
        }

        if ((uint32_t)p_tag + TAG_SIZE + p_tag->size >
            (uint32_t)gp_heap_start + HEAP_SIZE) {
            panic_enter();
            kprintf("heap: check_tags: chunk of tag %P ends beyond heap at"
                    " 0x%08X\n",
                    p_tag, (((uint32_t)p_tag) + sizeof(*p_tag) + p_tag->size));
            b_panic = true;
            break;
        }
    }

    if (b_panic) { panic("invalid heap state"); }
}
