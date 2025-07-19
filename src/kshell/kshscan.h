#pragma once

#include <stddef.h>

#include "list.h"

#define kshscanMAX_ARG_LEN 128

typedef enum {
    KSHSCAN_ERR_NONE,
    KSHSCAN_ERR_EXP_SINGLE_QUOTE,
    KSHSCAN_ERR_EXP_DOUBLE_QUOTE,
} kshscan_err_type_t;

typedef struct {
    kshscan_err_type_t err_type;
    size_t char_pos;
} kshscan_err_t;

typedef struct {
    list_node_t list_node;
    char *arg_str;
} kshscan_arg_t;

kshscan_err_t kshscan_str(const char *str, list_t *arg_list);
void kshscan_free_arg(kshscan_arg_t *arg);
void kshscan_free_arg_list(list_t *arg_list);
