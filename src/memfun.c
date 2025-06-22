#include "memfun.h"

typedef int si128_t [[gnu::vector_size(16), gnu::aligned(16)]];

static void memmove_si128(si128_t *p_dest, const si128_t *p_src,
                          size_t num_si128);
static void memclr_si128(si128_t *p_dest, size_t num_si128);

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes) {
    __asm__ volatile("rep movsb"
                     : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                     : "0"(p_dest), "1"(p_src), "2"(num_bytes)
                     : "memory");
    return p_dest;
}

void *kmemmove(void *p_dest, const void *p_src, size_t num_bytes) {
    if (p_dest < p_src) {
        __asm__ volatile("rep movsb"
                         : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                         : "0"(p_dest), "1"(p_src), "2"(num_bytes)
                         : "memory");
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
                         : "memory");
    }
    return p_dest;
}

void *kmemset(void *p_dest, int ch, size_t num_bytes) {
    __asm__ volatile("rep stosb"
                     : "=D"(p_dest), "=a"(ch), "=c"(num_bytes)
                     : "0"(p_dest), "1"(ch), "2"(num_bytes)
                     : "memory");
    return p_dest;
}

void *kmemset_word(void *p_dest, uint16_t word, size_t num_bytes) {
    __asm__ volatile("rep stosw"
                     : "=D"(p_dest), "=a"(word), "=c"(num_bytes)
                     : "0"(p_dest), "1"(word), "2"(num_bytes)
                     : "memory");
    return p_dest;
}

/*
 * Move `num_bytes` bytes from `p_src` to `p_dest` using SIMD instructions.
 *
 * NOTE: `p_dest` and `p_src` must have the same alignment, otherwise a GP#
 * exception is generated.
 */
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

/*
 * Zeroes `num_bytes` bytes starting at `p_dest` using SIMD instructions.
 */
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

/*
 * Moves `num_si128` 128-bit SIMD integers from `p_src` to `p_dest` using SSE2
 * instructions.
 *
 * NOTE: `p_dest` and `p_src` must be 16-byte aligned.
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

/*
 * Zeroes `num_si128` 128-bit SIMD integers starting at `p_dest` using SSE2
 * instructions.
 *
 * NOTE: `p_dest` must be 16-byte aligned, otherwise a GP# exception is
 * generated.
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
