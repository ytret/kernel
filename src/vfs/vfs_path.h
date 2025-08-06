#pragma once

#include "list.h"
#include "vfs/vfs_err.h"

#define VFS_PATH_MAX_PARTS 256

typedef struct {
    list_t parts;
    bool is_absolute;
} vfs_path_t;

typedef struct {
    list_node_t list_node;
    char *name;
} vfs_path_part_t;

vfs_err_t vfs_path_from_str(const char *path, vfs_path_t *out_path);
void vfs_path_free(vfs_path_t *path);
