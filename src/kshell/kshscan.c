#include <stddef.h>

#include "heap.h"
#include "kshell/kshscan.h"
#include "kstring.h"
#include "memfun.h"

static void prv_kshscan_add_arg(list_t *arg_list, const char *arg_buf,
                                size_t len);

kshscan_err_t kshscan_str(const char *str, list_t *arg_list) {
    kshscan_err_t err = {
        .err_type = KSHSCAN_ERR_NONE,
        .char_pos = 0,
    };

    char *const arg_buf = heap_alloc(kshscanMAX_ARG_LEN + 1);
    size_t arg_len = 0;
    bool add_arg = false;

    bool escaped = false;
    char quot = 0; // 0 is used as 'not inside quotation marks'

    for (size_t idx = 0; idx < string_len(str); idx++) {
        const char ch = str[idx];
        const char next_ch = str[idx + 1];

        bool add_ch = true;

        if (ch == '\\') {
            add_arg = true;
            if (!escaped && (next_ch == ' ' || next_ch == '\\' ||
                             next_ch == '\'' || next_ch == '"')) {
                escaped = true;
                add_ch = false;
            } else if (escaped) {
                escaped = false;
            }
        } else if ((ch == '\'' || ch == '"') && (!quot || ch == quot)) {
            add_arg = true;
            if (escaped) {
                escaped = false;
            } else if (ch == quot) {
                quot = 0;
                add_ch = false;
            } else {
                quot = ch;
                add_ch = false;
            }
        } else if (ch == ' ' && !quot) {
            if (escaped) {
                escaped = false;
            } else {
                if (add_arg) {
                    add_arg = false;
                    prv_kshscan_add_arg(arg_list, arg_buf, arg_len);
                }
                arg_len = 0;
                add_ch = false;
            }
        }

        if (add_ch) {
            arg_buf[arg_len] = ch;
            arg_len++;
            add_arg = true;
        }
    }

    if (quot) {
        err.err_type = quot == '\'' ? KSHSCAN_ERR_EXP_SINGLE_QUOTE
                                    : KSHSCAN_ERR_EXP_DOUBLE_QUOTE;
        err.char_pos = string_len(str);
    }

    if (add_arg) { prv_kshscan_add_arg(arg_list, arg_buf, arg_len); }

    heap_free(arg_buf);
    return err;
}

void kshscan_free_arg(kshscan_arg_t *arg) {
    if (arg) {
        heap_free(arg->arg_str);
        heap_free(arg);
    }
}

void kshscan_free_arg_list(list_t *arg_list) {
    list_node_t *arg_node;
    while ((arg_node = list_pop_first(arg_list))) {
        kshscan_arg_t *const arg =
            LIST_NODE_TO_STRUCT(arg_node, kshscan_arg_t, list_node);
        kshscan_free_arg(arg);
    }
}

static void prv_kshscan_add_arg(list_t *arg_list, const char *arg_buf,
                                size_t len) {
    char *const arg_copy = heap_alloc(len + 1);
    kmemcpy(arg_copy, arg_buf, len);
    arg_copy[len] = 0;

    kshscan_arg_t *const arg_item = heap_alloc(sizeof(*arg_item));
    arg_item->arg_str = arg_copy;
    list_append(arg_list, &arg_item->list_node);
}
