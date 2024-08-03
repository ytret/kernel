#include "memfun.h"

void *memcpy(void *p_dest, const void *p_src, size_t num_bytes) {
    __asm__ volatile("rep movsb"
                     : "=D"(p_dest), "=S"(p_src), "=c"(num_bytes)
                     : "0"(p_dest), "1"(p_src), "2"(num_bytes)
                     : "memory");
    return p_dest;
}

void *memmove(void *p_dest, const void *p_src, size_t num_bytes) {
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

void *memset(void *p_dest, int ch, size_t num_bytes) {
    __asm__ volatile("rep stosb"
                     : "=D"(p_dest), "=a"(ch), "=c"(num_bytes)
                     : "0"(p_dest), "1"(ch), "2"(num_bytes)
                     : "memory");
    return p_dest;
}
