#include <stddef.h>

#include "heap.h"
#include "memfun.h"
#include "vfs/vnode.h"
#include "vfs/vpath.h"

vpath_err_t vpath_from_str(const char *path_str, vpath_t *out_path) {
    kmemset(out_path, 0, sizeof(*out_path));
    list_init(&out_path->parts, NULL);

    size_t idx = 0;
    size_t substr_off = 0;
    size_t substr_len = 0;
    size_t num_parts = 0;

    for (;;) {
        const char ch = path_str[idx];
        if (ch == '/' || ch == 0) {
            if ((substr_len + 1) > VNODE_MAX_NAME_SIZE) {
                return VPATH_ERR_PART_TOO_LONG;
            }
            if (num_parts >= VPATH_MAX_PARTS) {
                return VPATH_ERR_TOO_MANY_PARTS;
            }
            if (idx == 0 && ch == 0) { return VPATH_ERR_EMPTY; }
            if (idx == 0) { out_path->is_absolute = true; }

            if (substr_len > 0) {
                char *const substr = heap_alloc(substr_len + 1);
                kmemcpy(substr, &path_str[substr_off], substr_len);
                substr[substr_len] = 0;

                vpath_part_t *const part = heap_alloc(sizeof(*part));
                kmemset(part, 0, sizeof(*part));
                part->name = substr;

                list_append(&out_path->parts, &part->list_node);
                num_parts++;
            }

            substr_off = idx + 1;
            substr_len = 0;

            if (path_str[idx] == 0) { break; }
        } else {
            substr_len++;
        }
        idx++;
    }

    return VPATH_ERR_NONE;
}

void vpath_free(vpath_t *path) {
    list_node_t *node;
    while ((node = list_pop_first(&path->parts))) {
        vpath_part_t *const part =
            LIST_NODE_TO_STRUCT(node, vpath_part_t, list_node);
        heap_free(part->name);
        heap_free(part);
    }
}

const char *vpath_err_str(vpath_err_t err) {
    switch (err) {
    case VPATH_ERR_NONE:
        return "VPATH_ERR_NONE";
    case VPATH_ERR_EMPTY:
        return "VPATH_ERR_EMPTY";
    case VPATH_ERR_TOO_MANY_PARTS:
        return "VPATH_ERR_TOO_MANY_PARTS";
    case VPATH_ERR_PART_TOO_LONG:
        return "VPATH_ERR_PART_TOO_LONG";
    case VPATH_ERR_MUST_BE_ABSOLUTE:
        return "VPATH_ERR_MUST_BE_ABSOLUTE";
    case VPATH_ERR_BAD_NODE:
        return "VPATH_ERR_BAD_NODE";
    }
    return "<unknown error>";
}
