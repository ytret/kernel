#pragma once

#include <stdbool.h>

/*
 * Converts a node pointer `p_node` to a pointer of its container type
 * `struct_type`. `node_name_in_struct` must be the name of the field in
 * `struct_type` that contains the pointer `p_node`.
 */
#define LIST_NODE_TO_STRUCT(p_node, struct_type, node_name_in_struct)          \
    ((struct_type *)(((uint8_t *)(p_node)) -                                   \
                     offsetof(struct_type, node_name_in_struct)))

struct list_node;

typedef struct list_node {
    struct list_node *p_next;
} list_node_t;

typedef struct {
    list_node_t *p_first_node;
    list_node_t *p_last_node;
} list_t;

void list_init(list_t *p_list, list_node_t *p_init_node);
void list_clear(list_t *p_list);
void list_append(list_t *p_list, list_node_t *p_node);
bool list_remove(list_t *p_list, list_node_t *p_node);
list_node_t *list_pop_first(list_t *p_list);
bool list_is_empty(list_t *p_list);
