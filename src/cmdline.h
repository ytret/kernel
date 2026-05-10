#pragma once

#include <stddef.h>

bool cmdline_init(const char *str);

size_t cmdline_num_values(const char *key);
size_t cmdline_get_value(const char *key, size_t value_idx, char *buf,
                         size_t buf_size);
