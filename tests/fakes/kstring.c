#include <string.h>

#include "kstring.h"

bool string_equals(char const *p_a, char const *p_b) {
    return strcmp(p_a, p_b) == 0;
}

size_t string_len(char const *p_str) {
    return strlen(p_str);
}

char *string_dup(char const *p_str) {
    return strdup(p_str);
}
