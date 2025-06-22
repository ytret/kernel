/**
 * @file list.c
 * Double-ended linked list implementation.
 */

#include <stddef.h>

#include "list.h"

void list_init(list_t *p_list, list_node_t *p_init_node) {
    __builtin_memset(p_list, 0, sizeof(*p_list));

    p_list->p_first_node = p_init_node;
    p_list->p_last_node = p_init_node;

    if (p_init_node) { p_list->p_first_node->p_next = NULL; }
}

void list_clear(list_t *p_list) {
    p_list->p_first_node = NULL;
    p_list->p_last_node = NULL;
}

void list_append(list_t *p_list, list_node_t *p_node) {
    if (p_list->p_last_node == NULL) {
        p_list->p_first_node = p_node;
    } else {
        p_list->p_last_node->p_next = p_node;
    }
    p_list->p_last_node = p_node;
    p_node->p_next = NULL;
}

bool list_remove(list_t *p_list, list_node_t *p_node) {
    list_node_t *p_iter = p_list->p_first_node;
    list_node_t *p_prev = NULL;
    while (p_iter != NULL) {
        if (p_iter == p_node) {
            if (p_prev) {
                p_prev->p_next = p_iter->p_next;
            } else {
                p_list->p_first_node = p_iter->p_next;
            }
            if (!p_iter->p_next) { p_list->p_last_node = p_prev; }
            return true;
        }
        p_prev = p_iter;
        p_iter = p_iter->p_next;
    }
    return false;
}

list_node_t *list_pop_first(list_t *p_list) {
    list_node_t *p_node = NULL;
    if (p_list->p_first_node) {
        p_node = p_list->p_first_node;
        p_list->p_first_node = p_list->p_first_node->p_next;
        p_node->p_next = NULL;

        if (p_list->p_last_node == p_node) { p_list->p_last_node = NULL; }
    }
    return p_node;
}

bool list_is_empty(list_t *p_list) {
    return p_list->p_first_node == NULL;
}
