#pragma once

#include "vfs/vfs_err.h"
#include "vfs/vnode.h"

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
     * @returns An error code, see #vfs_err_t.
     */
    vfs_err_t (*f_mount)(void *ctx, vnode_t *node);

    /**
     * Unmount the filesystem @a ctx from the VFS node @a node.
     *
     * @param ctx  Filesystem context pointer.
     * @param node VFS node, which the filesystem is currently mounted at.
     *
     * @returns An error code, see #vfs_err_t.
     */
    vfs_err_t (*f_unmount)(void *ctx, vnode_t *node);
} vfs_fs_desc_t;
