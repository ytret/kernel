#pragma once

#include "kerr.h"
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

kerr_t vpath_from_str(const char *path, vpath_t *out_path);
void vpath_free(vpath_t *path);
