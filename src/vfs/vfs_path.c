#include <stddef.h>

#include "heap.h"
#include "memfun.h"
#include "vfs/vfs_node.h"
#include "vfs/vfs_path.h"

vfs_err_t vfs_path_from_str(const char *path_str, vfs_path_t *out_path) {
    kmemset(out_path, 0, sizeof(*out_path));
    list_init(&out_path->parts, NULL);

    size_t idx = 0;
    size_t substr_off = 0;
    size_t substr_len = 0;
    size_t num_parts = 0;

    for (;;) {
        const char ch = path_str[idx];
        if (ch == '/' || ch == 0) {
            if ((substr_len + 1) > VFS_NODE_MAX_NAME_SIZE) {
                return VFS_ERR_PATH_PART_TOO_LONG;
            }
            if (num_parts >= VFS_PATH_MAX_PARTS) {
                return VFS_ERR_PATH_TOO_MANY_PARTS;
            }
            if (idx == 0 && ch == 0) { return VFS_ERR_PATH_EMPTY; }
            if (idx == 0) { out_path->is_absolute = true; }

            if (substr_len > 0) {
                char *const substr = heap_alloc(substr_len + 1);
                kmemcpy(substr, &path_str[substr_off], substr_len);
                substr[substr_len] = 0;

                vfs_path_part_t *const part = heap_alloc(sizeof(*part));
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

    return VFS_ERR_NONE;
}

void vfs_path_free(vfs_path_t *path) {
    list_node_t *node;
    while ((node = list_pop_first(&path->parts))) {
        vfs_path_part_t *const part =
            LIST_NODE_TO_STRUCT(node, vfs_path_part_t, list_node);
        heap_free(part->name);
        heap_free(part);
    }
}
