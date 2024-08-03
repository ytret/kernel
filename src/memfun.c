#include <stdint.h>

#include "memfun.h"

void *memcpy(void *p_dest, void const *p_src, size_t num_bytes) {
    uint8_t *p_dest_u8 = (uint8_t *)p_dest;
    uint8_t const *p_src_u8 = (uint8_t const *)p_src;

    for (size_t idx = 0; idx < num_bytes; idx++) {
        p_dest_u8[idx] = p_src_u8[idx];
    }

    return p_dest;
}

void *memmove(void *p_dest, void const *p_src, size_t num_bytes) {
    uint8_t *p_dest_u8 = (uint8_t *)p_dest;
    uint8_t const *p_src_u8 = (uint8_t const *)p_src;

    if (p_dest < p_src) {
        for (size_t idx = 0; idx < num_bytes; idx++) {
            p_dest_u8[idx] = p_src_u8[idx];
        }
    } else if (p_src < p_dest) {
        for (size_t idx = 0; idx < num_bytes; idx++) {
            p_dest_u8[(num_bytes - 1) - idx] = p_src_u8[(num_bytes - 1) - idx];
        }
    }

    return p_dest;
}

void *memset(void *p_dest, int ch, size_t num_bytes) {
    // NOTE: void pointer arithmetic is a GNU extension.

    if (__builtin_expect(num_bytes == 0, 0)) { return p_dest; }

    const uint8_t by = (uint8_t)ch;
    void *const p_end = p_dest + num_bytes;
    void *const p_start_dword = (void *)(((uintptr_t)p_dest + 3) & ~3);
    void *p_dword = p_start_dword;

    // Set the dword-addressable part.
    if (__builtin_expect(p_start_dword >= p_dest, 1)) {
        uint32_t dword = (by << 24) | (by << 16) | (by << 8) | by;
        while (p_dword <= (p_end - 4)) {
            *(uint32_t *)p_dword = dword;
            p_dword += 4;
        }
    }

    // Set the first non-dword-addressable bytes.
    while (p_dest < p_start_dword && p_dest < p_end) {
        *(uint8_t *)p_dest = by;
        p_dest++;
    }

    // Set the last non-dword addressable bytes.
    p_dest = (void *)((uintptr_t)p_end & ~3);
    while (p_dest < p_end) {
        *(uint8_t *)p_dest = by;
        p_dest++;
    }

    return p_dest;
}
