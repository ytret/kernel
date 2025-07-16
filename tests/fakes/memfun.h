#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *kmemcpy(void *p_dest, const void *p_src, size_t num_bytes);
void *kmemset(void *p_dest, int ch, size_t num_bytes);

#ifdef __cplusplus
}
#endif
