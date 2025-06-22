/**
 * @file list.h
 * Double-ended linked list.
 */

#pragma once

#include <stdbool.h>

/**
 * Converts @a p_node to a pointer to its container type @a struct_type.
 *
 * @param p_node              Node pointer.
 * @param struct_type         Type of struct that contains `*p_node`.
 * @param node_name_in_struct Name of the node field in @a struct_type.
 *
 * Example usage:
 * ```
 * struct container {
 *     int foo;
 *     list_node_t node; // May be anywhere in the struct.
 *     int bar;
 * };
 * struct container some_container = {};
 * list_node_t *node_ptr = &some_container.node;
 * struct container *p_container =
 *     LIST_NODE_TO_STRUCT(node_ptr,         // p_node
 *                         struct container, // struct_type
 *                         node);            // node_name_in_struct
 * assert(p_container == &some_container);
 * ```
 *
 * This way one can use `node` values to chain `struct container` values into a
 * linked list, traverse through the list, and get back to the
 * `struct container` values.
 */
#define LIST_NODE_TO_STRUCT(p_node, struct_type, node_name_in_struct)          \
    ((struct_type *)(((uint8_t *)(p_node)) -                                   \
                     offsetof(struct_type, node_name_in_struct)))

/**
 * Finds the first node in @a p_list that satisfies @a found_expr.
 *
 * @param p_list              List pointer to search through.
 * @param p_found_struct      Pointer that will store the found container.
 * @param struct_type         Type of container struct.
 * @param node_name_in_struct Name of the node field in @a struct_type.
 * @param found_expr          Test expression.
 * @param var_in_expr         Container identifier in @a found_expr.
 *
 * Iterates through every node of @a p_list, from start to end, until it finds a
 * container that satisfies @a found_expr.
 *
 * See #LIST_NODE_TO_STRUCT for explanation of @a p_found_struct, @a
 * struct_type, and @a node_name_in_struct.
 *
 * Example usage:
 * ```
 * struct container {
 *     int foo;
 *     list_node_t node;
 * };
 * list_t some_list;
 * // ... fill in some_list with nodes...
 * struct container *p_container;
 * LIST_FIND(&some_list,             // p_list
 *           p_container,            // p_found_struct
 *           struct container,       // struct_type
 *           node,                   // node_name_in_struct
 *           it_container->foo == 2, // found_expr
 *           it_container);          // var_in_expr
 * // At this point, if some_list had a struct with foo equal to 2, p_container
 * // will point to that struct. Otherwise, it will be NULL.
 * ```
 */
#define LIST_FIND(p_list, p_found_struct, struct_type, node_name_in_struct,    \
                  found_expr, var_in_expr)                                     \
    do {                                                                       \
        p_found_struct = NULL;                                                 \
        list_node_t *__p_node = (p_list)->p_first_node;                        \
        while (__p_node != NULL) {                                             \
            struct_type *var_in_expr = LIST_NODE_TO_STRUCT(                    \
                __p_node, struct_type, node_name_in_struct);                   \
            if (found_expr) {                                                  \
                p_found_struct = var_in_expr;                                  \
                break;                                                         \
            }                                                                  \
            __p_node = __p_node->p_next;                                       \
        }                                                                      \
    } while (0);

struct list_node;

typedef struct list_node {
    struct list_node *p_next;
} list_node_t;

typedef struct {
    list_node_t *p_first_node;
    list_node_t *p_last_node;
} list_t;

/**
 * Initializes @a p_list with an optional first node.
 * @param p_list      List pointer.
 * @param p_init_node Optional first node (may be `NULL`).
 */
void list_init(list_t *p_list, list_node_t *p_init_node);

/**
 * Clears @a p_list without deallocating its nodes.
 * @param p_list List pointer.
 */
void list_clear(list_t *p_list);

/**
 * Appends @a p_node to the end of @a p_list.
 * @param p_list List pointer.
 * @param p_node Node to add.
 */
void list_append(list_t *p_list, list_node_t *p_node);

/**
 * Removes @a p_node from @a p_list.
 * @param p_list List pointer.
 * @param p_node Node to remove.
 * @returns `true` if the node has been found and removed, `false` otherwise.
 */
bool list_remove(list_t *p_list, list_node_t *p_node);

/**
 * Removes the first node from @a p_list and returns it.
 * @param p_list List pointer.
 * @returns
 * - First node from the list if it had any.
 * - `NULL` if the list is empty.
 */
list_node_t *list_pop_first(list_t *p_list);

/**
 * Returns `true` if @a p_list is empty (has no nodes).
 * @param p_list List pointer.
 */
bool list_is_empty(list_t *p_list);
