#pragma once

#include "dynarr.h"

typedef struct {
    char *key;
    size_t key_len;
    char *value;
    size_t value_len;
} cmdline_item_t;

typedef struct {
    dynarr_t items; // item type: cmdline_item_t
} cmdline_t;

bool cmdline_init(const char *str);

bool cmdline_has_key(const char *key);
size_t cmdline_get_value(const char *key, char *buf, size_t buf_size);
