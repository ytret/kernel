#include "fs/ramfs.h"
#include "heap.h"
#include "test/ktest.h"
#include "test/ktest_smp.h"
#include "vfs/vnode.h"

KTEST_SUITE(KTEST_SMP, SMPSuiteVNode);

static void SMPSuiteVNode_RefCount_job_fn(void *arg) {
    vnode_t *const vnode = arg;
    for (size_t idx = 0; idx < 1000000; idx++) {
        vnode_ref(vnode);
        // TODO: random pause
        vnode_put(vnode);
    }
}

static ktest_smpjob_t SMPSuiteVNode_RefCount_job = {
    .name = "SMPSuiteVNode_RefCount",
    .fn = SMPSuiteVNode_RefCount_job_fn,
};

KTEST(SMPSuiteVNode, RefCount) {
    ramfs_ctx_t fs_ctx;
    ramfs_init(&fs_ctx, SIZE_MAX);

    ramfs_node_t *node = NULL;

    vnode_t *vnode = NULL;
    vnode_t *const root = vnode_root_node();
    KTEST_ASSERT_NE(root, NULL);

    const vfs_err_t mountfs_err = ramfs_get_desc()->f_mount(&fs_ctx, root);
    KTEST_ASSERT_EQ(mountfs_err, VFS_ERR_NONE);
    KTEST_ASSERT_NE(root->ops, NULL);
    KTEST_ASSERT_NE(root->ops->f_mknode, NULL);

    const vfs_err_t mknode_err =
        root->ops->f_mknode(root, &vnode, "vnode", VNODE_FILE);
    KTEST_ASSERT_EQ(mknode_err, VFS_ERR_NONE);
    KTEST_ASSERT_EQ(vnode->refcount, 1);

    node = vnode->fs_data;
    KTEST_ASSERT_NE(node, NULL);

    SMPSuiteVNode_RefCount_job.fn_arg = vnode;
    ktest_smpjob_broadcast(&SMPSuiteVNode_RefCount_job);
    ktest_smpjob_wait(&SMPSuiteVNode_RefCount_job);

    KTEST_ASSERT_EQ(vnode->refcount, 1);
    vnode_put(vnode);

    KTEST_ASSERT_EQ(vnode->refcount, 0);
    KTEST_ASSERT_EQ(vnode->magic, VNODE_MAGIC_DEAD);
    KTEST_ASSERT_EQ(vnode_get_destroy_cnt(), 1);
    KTEST_ASSERT(!node->deleted);

cleanup:
    if (vnode) { heap_free(vnode); }
    if (node) { ramfs_free_node(&fs_ctx, node); }
}
