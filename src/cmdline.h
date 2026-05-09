#pragma once

#include <stddef.h>

bool cmdline_init(const char *str);

bool cmdline_has_key(const char *key);
size_t cmdline_get_value(const char *key, char *buf, size_t buf_size);
