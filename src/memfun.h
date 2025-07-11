#pragma once

#include <stddef.h>
#include <stdint.h>

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemmove(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemset(void *p_dest, int ch, size_t num_bytes);
void *kmemset_word(void *p_dest, uint16_t word, size_t num_words);
int kmemcmp(const void *buf1, const void *buf2, size_t num_bytes);

/**
 * Moves @a num_bytes bytes from @a p_src to @a p_dest using SIMD instructions.
 *
 * @param p_dest    Destination address, may be unaligned.
 * @param p_src     Source address, may be unaligned.
 * @param num_bytes Number of bytes to copy, not necessarily 4-byte aligned.
 *
 * @warning
 * @a p_dest and @a p_src must have the same alignment, otherwise a GP#
 * exception is raised.
 */
void *kmemmove_sse2(void *p_dest, const void *p_src, size_t num_bytes);

/**
 * Zeroes out @a num_bytes bytes starting at @a p_dest using SIMD instructions.
 *
 * @param p_dest    Start address, may be unaligned.
 * @param num_bytes Number of bytes to zero out, not necessarily 4-byte aligned.
 */
void *kmemclr_sse2(void *p_dest, size_t num_bytes);

/**
 * Reads a volatile dword from @a p_src and copies it to @a p_dest.
 *
 * @param p_dest Destination buffer.
 * @param p_src  Volatile dword pointer.
 */
void kmemread_v4(void *p_dest, const volatile uint32_t *p_src);

/**
 * Writes a volatile dword at @a p_dest with a dword copied from @a p_src.
 *
 * @param p_dest Volatile dword pointer.
 * @param p_src  Source buffer.
 */
void kmemwrite_v4(volatile uint32_t *p_dest, const void *p_src);
