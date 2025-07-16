#include <string.h>

#include "memfun.h"

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes) {
    return memcpy(p_dest, p_src, num_bytes);
}

void *kmemset(void *p_dest, int ch, size_t num_bytes) {
    return memset(p_dest, ch, num_bytes);
}
