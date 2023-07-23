#pragma once

#include <stdbool.h>
#include <stddef.h>

void   string_to_upper(char * p_str);
bool   string_equals(char const * p_a, char const * p_b);
size_t string_len(char const * p_str);
size_t string_split(char const * p_str, char ch, bool b_ignore_empty,
                    char ** pp_res, size_t res_len);
