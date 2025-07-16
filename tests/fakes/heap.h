#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *heap_alloc(size_t num_bytes);
void heap_free(void *p_addr);

#ifdef __cplusplus
}
#endif
