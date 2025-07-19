#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool string_equals(char const *p_a, char const *p_b);
size_t string_len(char const *p_str);
char *string_dup(char const *p_str);

#ifdef __cplusplus
}
#endif
