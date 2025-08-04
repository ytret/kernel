#pragma once

#include <stddef.h>

#include "list.h"

typedef struct {
    const char *name;
    void (*f_handler)(list_t *arg_list);
    const char *help_str;
} kshell_cmd_t;

void kshcmd_parse(char const *p_cmd);
void kshcmd_get_cmds(const kshell_cmd_t **out_cmds, size_t *out_num_cmds);
