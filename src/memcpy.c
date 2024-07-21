#include <stdint.h>

#include "memcpy.h"

void *memcpy(void *p_dest, void const *p_src, size_t num_bytes) {
    uint8_t *p_dest_u8 = (uint8_t *)p_dest;
    uint8_t const *p_src_u8 = (uint8_t const *)p_src;

    for (size_t idx = 0; idx < num_bytes; idx++) {
        p_dest_u8[idx] = p_src_u8[idx];
    }

    return (p_dest);
}
