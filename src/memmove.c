#include <stdint.h>

#include "memmove.h"

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
