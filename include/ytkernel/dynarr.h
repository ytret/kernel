#pragma once

#include <stddef.h>

#define DYNARR_ITEM_ALIGN 8

typedef struct {
    void *buf;
    size_t buf_size;

    size_t item_real_size;
    size_t item_size;
    size_t item_align;

    size_t num_items;
    size_t cap_items;
} dynarr_t;

void dynarr_init(dynarr_t *arr, size_t item_size, size_t item_align,
                 size_t init_cap);

bool dynarr_push(dynarr_t *arr, const void *item, size_t *idx);
bool dynarr_insert_at(dynarr_t *arr, size_t idx, const void *item);

bool dynarr_pop(dynarr_t *arr, void *buf, size_t buf_size);
bool dynarr_take_at(dynarr_t *arr, size_t idx, void *buf, size_t buf_size);
bool dynarr_remove_at(dynarr_t *arr, size_t idx);

bool dynarr_get_at(dynarr_t *arr, size_t idx, void *buf, size_t buf_size);
const void *dynarr_ptr_at(const dynarr_t *arr, size_t idx);
