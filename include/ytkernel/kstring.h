#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Converts an ASCII string @a p_str to uppercase in place.
 * @param p_str NUL-terminated ASCII string.
 */
void string_to_upper(char *p_str);

/**
 * Compares strings @a p_a and @a p_b for byte-level equality.
 * @param p_a Left-hand NUL-terminated string.
 * @param p_b Right-hand NUL-terminated string.
 * @returns `true`, if each byte in @a p_a is equal to the same-indexed byte in
 * @a p_b. Otherwise, `false`.
 */
bool string_equals(char const *p_a, char const *p_b);

/**
 * Returns the length of an ASCII string @a p_str.
 * @param p_str NUL-terminated ASCII string.
 * @returns Number of bytes in @a p_str, not counting the final NUL byte.
 */
size_t string_len(char const *p_str);

/**
 * Splits a string @a p_str on each @a ch character.
 *
 * @param p_str          NUL-terminated string to split.
 * @param ch             Separator character.
 * @param b_ignore_empty If `true`, empty substrings are ignored.
 * @param pp_res         Buffer to store substrings (must not be NULL).
 * @param res_len        Maximum number of substrings in @a pp_res.
 *
 * @returns
 * The number of elements written to @a pp_res (may be zero), otherwise
 * `res_len + 1`, if there are more substrings to store than @a res_len allows.
 *
 * @note
 * String pointers written to @a pp_res are allocated on the heap and must be
 * freed by the caller.
 */
size_t string_split(char const *p_str, char ch, bool b_ignore_empty,
                    char **pp_res, size_t res_len);

/**
 * Converts a string @a p_str to a 32-bit unsigned integer in @a *p_num.
 *
 * @param p_str NUL-terminated ASCII string to convert.
 * @param p_num Result pointer (must not be NULL).
 * @param base  Number base: 2, 8, 10, or 16.
 *
 * @returns `true` if the converted number has been stored in @a p_num.
 *
 * @note
 * @a p_str must **not** contain a prefix, e.g. `0b`, `0o`, or `0x`.
 */
bool string_to_uint32(char const *p_str, uint32_t *p_num, int base);

/**
 * Converts a number @a num to a NUL-terminated ASCII string in @a p_buf.
 *
 * In case of hexadecimal numbers, digits A…F are written as lowercase
 * characters.
 *
 * @param num      Number to convert.
 * @param b_signed `true` - interpret @a num as signed, `false` - as unsigned.
 * @param p_buf    Buffer to store the converted string in.
 * @param base     Base in which to represent the number in @a p_buf.
 *
 * @returns
 * The number of bytes written to @a p_buf, including the final NUL byte.
 *
 * @note
 * The base prefix (e.g., `0x`) is not written to @a p_buf.
 *
 * @warning
 * - @a p_buf must be large enough to store the string representation of @a num
 *   in @a base.
 * - Do not forget to count the possible minus sign in case if @a b_signed is
 *   `true`.
 */
size_t string_itoa(unsigned int num, bool b_signed, char *p_buf,
                   unsigned int base);

/**
 * Copies the string @a p_str on the heap.
 * @param p_str NUL-terminated string.
 * @returns A heap-allocated string, a copy of @a p_str.
 */
char *string_dup(char const *p_str);
