#pragma once

#include "kerr.h"

typedef struct vnode vnode_t;

/**
 * Filesystem descriptor.
 *
 * Each filesystem has only one struct of this type.
 */
typedef struct {
    /**
     * ASCII filesystem name.
     *
     * Ideally without spaces to remind of a variable, like "ramfs", "ext4",
     * etc.
     */
    const char *name;

    /**
     * Mount the filesystem @a ctx at the VFS node @a node.
     *
     * @param ctx  Filesystem context pointer.
     * @param node VFS node to mount the filesystem at.
     *
     * @returns An error code, see #kerr_t.
     */
    kerr_t (*f_mount)(void *ctx, vnode_t *node);

    /**
     * Unmount the filesystem @a ctx from the VFS node @a node.
     *
     * @param ctx  Filesystem context pointer.
     * @param node VFS node, which the filesystem is currently mounted at.
     *
     * @returns An error code, see #kerr_t.
     */
    kerr_t (*f_unmount)(void *ctx, vnode_t *node);

    /**
     * Function called right before a vnode is freed.
     *
     * The intention is to let the file system driver to unset the weak vnode
     * pointer in its corresponding node structure.
     *
     * @param ctx  Filesystem context pointer.
     * @param node VFS node to be freed.
     */
    void (*f_on_free_vnode)(void *ctx, vnode_t *node);
} fs_desc_t;
