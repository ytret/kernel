#include <stdint.h>

#include "assert.h"
#include "dynarr.h"
#include "heap.h"
#include "memfun.h"

#define DYNARR_BUMP_CAP 8
#define DYNARR_ITEM_PTR(arr, idx)                                              \
    (void *)((uintptr_t)(arr)->buf + (idx) * arr->item_size)

static void prv_dynarr_alloc_cap(dynarr_t *arr, size_t new_cap);

void dynarr_init(dynarr_t *arr, size_t item_size, size_t item_align,
                 size_t init_cap) {
    ASSERT(item_align > 0);
    ASSERT((item_align & (item_align - 1)) == 0);

    kmemset(arr, 0, sizeof(*arr));

    arr->item_real_size = item_size;
    arr->item_size = (item_size + (item_align - 1)) & ~(item_align - 1);
    arr->item_align = item_align;

    prv_dynarr_alloc_cap(arr, init_cap);
}

bool dynarr_push(dynarr_t *arr, const void *item, size_t *idx) {
    if (!item) { return false; }

    if (arr->num_items + 1 > arr->cap_items) {
        prv_dynarr_alloc_cap(arr, arr->cap_items + DYNARR_BUMP_CAP);
    }

    const size_t new_idx = arr->num_items;
    if (!dynarr_insert_at(arr, new_idx, item)) { return false; }
    if (idx) { *idx = new_idx; }

    return true;
}

bool dynarr_insert_at(dynarr_t *arr, size_t idx, const void *item) {
    if (!item) { return false; }

    if (arr->num_items + 1 > arr->cap_items) {
        // TODO: there is a potential optimization - do not copy all items, but
        // only those preceding the item at idx.
        prv_dynarr_alloc_cap(arr, arr->cap_items + DYNARR_BUMP_CAP);
    }

    if (idx != arr->num_items) {
        void *const dest = DYNARR_ITEM_PTR(arr, idx + 1);
        const void *const src = DYNARR_ITEM_PTR(arr, idx);
        kmemmove(dest, src, (arr->num_items - idx) * arr->item_size);
    }

    void *const dest = DYNARR_ITEM_PTR(arr, idx);
    kmemset(dest, 0, arr->item_size);
    kmemcpy(dest, item, arr->item_real_size);

    arr->num_items++;

    return true;
}

bool dynarr_pop(dynarr_t *arr, void *buf, size_t buf_size) {
    if (arr->num_items == 0) { return false; }
    return dynarr_take_at(arr, arr->num_items - 1, buf, buf_size);
}

bool dynarr_take_at(dynarr_t *arr, size_t idx, void *buf, size_t buf_size) {
    if (idx >= arr->num_items) { return false; }
    if (buf && buf_size < arr->item_real_size) { return false; }

    if (buf) {
        const void *const src = DYNARR_ITEM_PTR(arr, idx);
        kmemcpy(buf, src, arr->item_real_size);
    }

    if (idx != arr->num_items - 1) {
        void *const dest = DYNARR_ITEM_PTR(arr, idx);
        const void *const src = DYNARR_ITEM_PTR(arr, idx + 1);
        kmemmove(dest, src, (arr->num_items - idx - 1) * arr->item_size);
    }

    arr->num_items--;

    return true;
}

bool dynarr_remove_at(dynarr_t *arr, size_t idx) {
    return dynarr_take_at(arr, idx, NULL, 0);
}

bool dynarr_get_at(dynarr_t *arr, size_t idx, void *buf, size_t buf_size) {
    if (idx >= arr->num_items) { return false; }
    if (!buf) { return false; }
    if (buf_size < arr->item_real_size) { return false; }

    const void *const src = DYNARR_ITEM_PTR(arr, idx);
    kmemcpy(buf, src, arr->item_real_size);

    return true;
}

static void prv_dynarr_alloc_cap(dynarr_t *arr, size_t new_cap) {
    arr->buf_size = new_cap * arr->item_size;
    if (arr->buf) {
        arr->buf = heap_realloc(arr->buf, arr->buf_size, arr->item_align);
    } else {
        arr->buf = heap_alloc_aligned(arr->buf_size, arr->item_align);
    }
    arr->cap_items = new_cap;
}
