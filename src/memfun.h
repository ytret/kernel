#pragma once

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *p_dest, const void *p_src, size_t num_bytes);
void *memmove(void *p_dest, const void *p_src, size_t num_bytes);
void *memset(void *p_dest, int ch, size_t num_bytes);
void *memset_word(void *p_dest, uint16_t word, size_t num_words);

void *memmove_sse2(void *p_dest, const void *p_src, size_t num_bytes);
void *memclr_sse2(void *p_dest, size_t num_bytes);
