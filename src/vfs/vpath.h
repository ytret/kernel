#pragma once

#include "list.h"

#define VPATH_MAX_PARTS 256

typedef struct {
    list_t parts;
    bool is_absolute;
} vpath_t;

typedef struct {
    list_node_t list_node;
    char *name;
} vpath_part_t;

typedef enum {
    VPATH_ERR_NONE,
    VPATH_ERR_EMPTY,
    VPATH_ERR_TOO_MANY_PARTS,
    VPATH_ERR_PART_TOO_LONG,
    VPATH_ERR_MUST_BE_ABSOLUTE,
    VPATH_ERR_BAD_NODE,
} vpath_err_t;

vpath_err_t vpath_from_str(const char *path, vpath_t *out_path);
void vpath_free(vpath_t *path);
const char *vpath_err_str(vpath_err_t err);
