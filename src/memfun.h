#pragma once

#include <stddef.h>
#include <stdint.h>

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemmove(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemset(void *p_dest, int ch, size_t num_bytes);
void *kmemset_word(void *p_dest, uint16_t word, size_t num_words);

void *kmemmove_sse2(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemclr_sse2(void *p_dest, size_t num_bytes);
