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

    bool escaped = false;
    char quot = 0; // 0 is used as 'not inside quotation marks'

    for (size_t idx = 0; idx < string_len(str); idx++) {
        const char ch = str[idx];
        const char next_ch = str[idx + 1];

        bool add_ch = true;

        if (ch == '\\') {
            if (!escaped && (next_ch == ' ' || next_ch == '\\' ||
                             next_ch == '\'' || next_ch == '"')) {
                escaped = true;
                add_ch = false;
            } else if (escaped) {
                escaped = false;
            }
        } else if ((ch == '\'' || ch == '"') && (!quot || ch == quot)) {
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
                prv_kshscan_add_arg(arg_list, arg_buf, arg_len);
                add_ch = false;
                arg_len = 0;
            }
        }

        if (add_ch) {
            arg_buf[arg_len] = ch;
            arg_len++;
        }
    }

    if (quot) {
        err.err_type = quot == '\'' ? KSHSCAN_ERR_EXP_SINGLE_QUOTE
                                    : KSHSCAN_ERR_EXP_DOUBLE_QUOTE;
        err.char_pos = string_len(str);
    }

    prv_kshscan_add_arg(arg_list, arg_buf, arg_len);

    heap_free(arg_buf);
    return err;
}

void kshscan_free_arg(kshscan_arg_t *arg) {
    if (arg) {
        heap_free(arg->arg_str);
        heap_free(arg);
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
