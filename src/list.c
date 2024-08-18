/*
 * Double-ended linked list.
 */

#include <stddef.h>

#include "list.h"

/*
 * Initializes a list with an optional first node.
 *
 * If `p_init_node` is NULL, the list is created empty.
 */
void list_init(list_t *p_list, list_node_t *p_init_node) {
    __builtin_memset(p_list, 0, sizeof(*p_list));

    p_list->p_first_node = p_init_node;
    p_list->p_last_node = p_init_node;

    if (p_init_node) { p_list->p_first_node->p_next = NULL; }
}

/*
 * Clears the list without deallocating its nodes.
 */
void list_clear(list_t *p_list) {
    p_list->p_first_node = NULL;
    p_list->p_last_node = NULL;
}

/*
 * Appends a node to the end of the list.
 */
void list_append(list_t *p_list, list_node_t *p_node) {
    if (p_list->p_last_node == NULL) {
        p_list->p_first_node = p_node;
    } else {
        p_list->p_last_node->p_next = p_node;
    }
    p_list->p_last_node = p_node;
    p_node->p_next = NULL;
}

/*
 * Removes a node from the list.
 *
 * Returns true if the node has been found and removed.
 */
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
            return true;
        }
        p_prev = p_iter;
        p_iter = p_iter->p_next;
    }
    return false;
}

/*
 * Returns the first node of the list and removes it from the list.
 *
 * If the list is empty, returns NULL.
 */
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
