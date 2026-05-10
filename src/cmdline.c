#include "assert.h"
#include "cmdline.h"
#include "dynarr.h"
#include "heap.h"
#include "kshell/kshscan.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"

typedef struct {
    char *key;
    size_t key_len;
    char *value;
    size_t value_len;
} cmdline_item_t;

typedef struct {
    dynarr_t items; // item type: cmdline_item_t
} cmdline_t;

static cmdline_t g_cmdline;

static bool prv_cmdline_add_kv(const char *kv);

bool cmdline_init(const char *str) {
    kmemset(&g_cmdline, 0, sizeof(g_cmdline));
    dynarr_init(&g_cmdline.items, sizeof(cmdline_item_t),
                _Alignof(cmdline_item_t), 0);

    // Reuse the kshscan scanning algorithm.
    list_t kv_list;
    list_init(&kv_list, NULL);
    const kshscan_err_t err = kshscan_str(str, &kv_list);
    if (err.err_type != KSHSCAN_ERR_NONE) {
        LOG_ERROR("ignore cmdline, kshscan error %d at char %zu", err.err_type,
                  err.char_pos);
        return false;
    }

    for (list_node_t *node = kv_list.p_first_node; node != NULL;
         node = node->p_next) {
        const kshscan_arg_t *const kv =
            LIST_NODE_TO_STRUCT(node, kshscan_arg_t, list_node);

        LOG_DEBUG("add cmdline item '%s'", kv->arg_str);

        const bool added = prv_cmdline_add_kv(kv->arg_str);
        if (!added) { return false; }
    }

    // All key-value pairs were added to `g_cmdline->items`.
    kshscan_free_arg_list(&kv_list);

    return true;
}

bool cmdline_has_key(const char *key) {
    DEBUG_ASSERT(key != NULL);

    for (size_t idx = 0; idx < g_cmdline.items.num_items; idx++) {
        const cmdline_item_t *const item = dynarr_ptr_at(&g_cmdline.items, idx);
        ASSERT(item != NULL);

        if (string_equals(key, item->key)) { return true; }
    }

    return false;
}

size_t cmdline_get_value(const char *key, char *buf, size_t buf_size) {
    DEBUG_ASSERT(key != NULL);

    if (!buf) { return 0; }

    for (size_t idx = 0; idx < g_cmdline.items.num_items; idx++) {
        const cmdline_item_t *const item = dynarr_ptr_at(&g_cmdline.items, idx);
        ASSERT(item != NULL);

        if (string_equals(key, item->key)) {
            const size_t copy_size = buf_size >= (item->value_len + 1)
                                         ? (item->value_len + 1)
                                         : buf_size;
            kmemcpy(buf, item->value, copy_size);
            return true;
        }
    }

    return false;
}

static bool prv_cmdline_add_kv(const char *kv) {
    cmdline_item_t item = {0};

    bool has_val = false;
    size_t value_idx = 0;
    size_t kv_len = 0;
    for (size_t idx = 0; kv[idx] != '\0'; idx++) {
        if (!has_val && kv[idx] == '=') {
            has_val = true;
            value_idx = idx + 1;
        }
        kv_len++;
    }

    if (has_val && value_idx == 1) {
        LOG_ERROR("cmdline key value pair '%s' has no key", kv);
        return false;
    }

    const size_t key_len = has_val ? value_idx - 1 : kv_len;
    item.key = heap_alloc(key_len + 1);
    kmemcpy(item.key, kv, key_len);
    item.key[key_len] = '\0';
    item.key_len = key_len;

    const size_t value_len = has_val ? kv_len - value_idx : 0;
    item.value = heap_alloc(value_len + 1);
    kmemcpy(item.value, &kv[value_idx], value_len);
    item.value[value_len] = '\0';
    item.value_len = value_len;

    if (!dynarr_push(&g_cmdline.items, &item, NULL)) {
        LOG_ERROR("dynarr_push failed for key-value pair '%s'", kv);
        return false;
    }

    return true;
}
