#include <stdint.h>

#include "memset.h"

void *memset(void *p_dest, int ch, size_t num_bytes) {
    uint8_t *p_dest_u8 = ((uint8_t *)p_dest);
    for (size_t idx = 0; idx < num_bytes; idx++) {
        p_dest_u8[idx] = ((char)ch);
    }
    return p_dest;
}
