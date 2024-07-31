#pragma once

#include <stddef.h>

void *memcpy(void *p_dest, void const *p_src, size_t num_bytes);
void *memmove(void *p_dest, void const *p_src, size_t num_bytes);
void *memset(void *p_dest, int ch, size_t num_bytes);
