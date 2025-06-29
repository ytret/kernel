#include "memfun.h"

typedef int si128_t [[gnu::vector_size(16), gnu::aligned(16)]];

/**
 * @{
 * @name Aliases for libgcc
 */
/// Alias for #kmemcpy().
[[gnu::alias("kmemcpy")]]
void *memcpy(void *p_dest, const void *p_src, size_t num_bytes);

/// Alias for #kmemmove().
[[gnu::alias("kmemmove")]]
void *memmove(void *p_dest, const void *p_src, size_t num_bytes);

/// Alias for #kmemset().
[[gnu::alias("kmemset")]]
void *memset(void *p_dest, int ch, size_t num_bytes);

/// Alias for #kmemcmp().
[[gnu::alias("kmemcmp")]]
int memcmp(const void *buf1, const void *buf2, size_t num_bytes);
/// @}

static void memmove_si128(si128_t *p_dest, const si128_t *p_src,
                          size_t num_si128);
static void memclr_si128(si128_t *p_dest, size_t num_si128);

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes) {
    __asm__ volatile("rep movsb"
                     : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                     : "0"(p_dest), "1"(p_src), "2"(num_bytes)
                     : "memory", "cc");
    return p_dest;
}

void *kmemmove(void *p_dest, const void *p_src, size_t num_bytes) {
    if (p_dest < p_src) {
        __asm__ volatile("rep movsb"
                         : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                         : "0"(p_dest), "1"(p_src), "2"(num_bytes)
                         : "memory", "cc");
    } else if (p_src < p_dest) {
        // NOTE: void pointer arithmetic is a GNU extension.
        void *p_dest_end = p_dest + num_bytes;
        const void *p_src_end = p_src + num_bytes;
        __asm__ volatile("std\n"
                         "rep movsb\n"
                         "cld"
                         : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                         : "0"(p_dest_end - 1), "1"(p_src_end - 1),
                           "2"(num_bytes)
                         : "memory", "cc");
    }
    return p_dest;
}

void *kmemset(void *p_dest, int ch, size_t num_bytes) {
    __asm__ volatile("rep stosb"
                     : "=D"(p_dest), "=a"(ch), "=c"(num_bytes)
                     : "0"(p_dest), "1"(ch), "2"(num_bytes)
                     : "memory", "cc");
    return p_dest;
}

void *kmemset_word(void *p_dest, uint16_t word, size_t num_bytes) {
    __asm__ volatile("rep stosw"
                     : "=D"(p_dest), "=a"(word), "=c"(num_bytes)
                     : "0"(p_dest), "1"(word), "2"(num_bytes)
                     : "memory", "cc");
    return p_dest;
}

int kmemcmp(const void *buf1, const void *buf2, size_t num_bytes) {
    // NOTE: the assembly below changes the value of 'buf1' and 'buf2', so this
    // (1) casts them to uint8_t *, (2) saves their original values.
    const uint8_t *buf1_u8 = buf1;
    const uint8_t *buf2_u8 = buf2;

    size_t cnt;
    bool eq;
    __asm__ volatile("cmp %%ecx, %%ecx\n"
                     "repe cmpsb"
                     : "=D"(buf1), "=S"(buf2), "=c"(cnt), "=@ccz"(eq)
                     : "0"(buf1), "1"(buf2), "2"(num_bytes)
                     : "memory", "cc");

    if (eq) {
        return 0;
    } else {
        const size_t first_neq = num_bytes - cnt - 1;
        return buf1_u8[first_neq] - buf2_u8[first_neq];
    }
}

void *kmemmove_sse2(void *p_dest, const void *p_src, size_t num_bytes) {
    void *const p_orig_dest = p_dest;

    // 1. Move the first non-16-byte-addresable part.
    size_t num_non_aligned = (16 - ((uintptr_t)p_dest & 15)) % 16;
    if (num_non_aligned > 0) {
        kmemmove(p_dest, p_src, num_non_aligned);
        p_dest += num_non_aligned;
        p_src += num_non_aligned;
        num_bytes -= num_non_aligned;
    }

    // 2. Move the 16-byte-addressable double qwords.
    size_t num_si128 = num_bytes / 16;
    if (num_si128 > 0) {
        memmove_si128(p_dest, p_src, num_si128);
        p_dest += num_si128 * 16;
        p_src += num_si128 * 16;
        num_bytes -= num_si128 * 16;
    }

    // 3. Move the remaining non-16-byte-addressable part.
    kmemmove(p_dest, p_src, num_bytes);

    return p_orig_dest;
}

void *kmemclr_sse2(void *p_dest, size_t num_bytes) {
    void *const p_orig_dest = p_dest;

    // 1. Set the first non-16-byte-addresable part.
    size_t num_non_aligned = (16 - ((uintptr_t)p_dest & 15)) % 16;
    if (num_non_aligned > 0) {
        kmemset(p_dest, 0, num_non_aligned);
        p_dest += num_non_aligned;
        num_bytes -= num_non_aligned;
    }

    // 2. Set the 16-byte-addressable double qwords.
    size_t num_si128 = num_bytes / 16;
    if (num_si128 > 0) {
        memclr_si128(p_dest, num_si128);
        p_dest += num_si128 * 16;
        num_bytes -= num_si128 * 16;
    }

    // 3. Set the remaining non-16-byte-addressable part.
    kmemset(p_dest, 0, num_bytes);

    return p_orig_dest;
}

/**
 * Moves a memory region using SSE2 instructions.
 *
 * @param p_dest    Destination address (16-byte aligned).
 * @param p_src     Source address (16-byte aligned).
 * @param num_si128 Number of 128-bit SIMD integers to copy.
 *
 * @warning
 * If @a p_dest and @a p_src are not 16-byte aligned, the CPU raises a GP#
 * exception.
 */
static void memmove_si128(si128_t *p_dest, const si128_t *p_src,
                          size_t num_si128) {
    si128_t *p_dest_end = p_dest + num_si128;
    const si128_t *p_src_end = p_src + num_si128;

    if (p_dest < p_src) {
        while (p_dest < p_dest_end) {
            __asm__ volatile("movdqa (%0), %%xmm0\n"
                             "movdqa %%xmm0, (%1)"
                             :                         /* output */
                             : "r"(p_src), "r"(p_dest) /* input */
                             : "memory"                /* clobbers */
                             // NOTE: %xmm0 cannot be clobbered because -msse2
                             // is not passed to GCC.
            );
            p_dest++;
            p_src++;
        }
    } else if (p_src < p_dest) {
        p_dest_end--;
        p_src_end--;
        while (p_dest_end >= p_dest) {
            __asm__ volatile("movdqa (%0), %%xmm0\n"
                             "movdqa %%xmm0, (%1)"
                             :                                 /* output */
                             : "r"(p_src_end), "r"(p_dest_end) /* input */
                             : "memory"                        /* clobbers */
                             // NOTE: %xmm0 cannot be clobbered because -msse2
                             // is not passed to GCC.
            );

            p_dest_end--;
            p_src_end--;
        }
    }
}

/**
 * Zeroes out a region using SSE2 instructions.
 *
 * @param p_dest    Start address (16-byte aligned).
 * @param num_si128 Number of SIMD integers to zero out.
 *
 * @warning
 * If @a p_dest is not 16-byte aligned, the CPU raises a GP# exception.
 */
static void memclr_si128(si128_t *p_dest, size_t num_si128) {
    __asm__ volatile(
        "xorpd %%xmm0, %%xmm0"
        : /* output */
        : /* input */
        : /* clobbers */
        // NOTE: %xmm0 cannot be clobbered because -msse2 is not passed to GCC.
    );

    si128_t *const p_dest_end = p_dest + num_si128;
    while (p_dest < p_dest_end) {
        __asm__ volatile("movdqa %%xmm0, (%0)"
                         :             /* output */
                         : "a"(p_dest) /* input */
                         : "memory"    /* clobbers */
        );
        p_dest++;
    }
}
