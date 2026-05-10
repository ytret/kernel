#include "fs/ramfs.h"
#include "heap.h"
#include "test/ktest.h"
#include "test/ktest_smp.h"
#include "vfs/vnode.h"

typedef struct {
    ramfs_ctx_t *fs;
    vnode_t *root;
} SMPSuiteVNode_ctx_t;

static SMPSuiteVNode_ctx_t g_SMPSuiteVNode_ctx;

static void SMPSuiteVNode_test_setup(ktest_testctx_t *testctx) {
    SMPSuiteVNode_ctx_t *const ctx = &g_SMPSuiteVNode_ctx;

    ctx->fs = ramfs_new(SIZE_MAX);
    KTEST_ASSERT_NE(ctx->fs, NULL);

    ctx->root = vnode_root_node();
    KTEST_ASSERT_NE(ctx->root, NULL);

    const vfs_err_t mountfs_err = ramfs_get_desc()->f_mount(ctx->fs, ctx->root);
    KTEST_ASSERT_EQ(mountfs_err, VFS_ERR_NONE);
    KTEST_ASSERT_NE(ctx->root->ops, NULL);
    KTEST_ASSERT_NE(ctx->root->ops->f_mknode, NULL);

cleanup:
    if (testctx->failed) {
        if (ctx->fs) { ramfs_free(ctx->fs); }
    }
}

static void SMPSuiteVNode_test_cleanup(ktest_testctx_t *testctx) {
    (void)testctx;

    SMPSuiteVNode_ctx_t *const ctx = &g_SMPSuiteVNode_ctx;
    if (ctx->fs) { ramfs_free(ctx->fs); }
}

KTEST_SUITE(KTEST_SMP, SMPSuiteVNode);

KTEST_SMPJOB(RefCountJob) {
    vnode_t *const vnode = arg;
    for (size_t idx = 0; idx < 1000000; idx++) {
        vnode_ref(vnode);
        // TODO: random pause
        vnode_put(vnode);
    }
}

KTEST(SMPSuiteVNode, RefCount) {
    SMPSuiteVNode_ctx_t *const ctx = &g_SMPSuiteVNode_ctx;
    ramfs_node_t *node = NULL;
    vnode_t *vnode = NULL;

    KTEST_PCALL(SMPSuiteVNode_test_setup);

    const vfs_err_t mknode_err =
        ctx->root->ops->f_mknode(ctx->root, &vnode, "vnode", VNODE_FILE);
    KTEST_ASSERT_EQ(mknode_err, VFS_ERR_NONE);
    KTEST_ASSERT_EQ(vnode->refcount, 1);

    node = vnode->fs_data;
    KTEST_ASSERT_NE(node, NULL);

    ktest_smpjob_broadcast(&KTEST_SMPJOB_REF(RefCountJob), vnode);
    KTEST_PCALL(ktest_smpjob_wait, &KTEST_SMPJOB_REF(RefCountJob));

    KTEST_ASSERT_EQ(vnode->refcount, 1);
    vnode_put(vnode);

    KTEST_ASSERT_EQ(vnode->refcount, 0);
    KTEST_ASSERT_EQ(vnode->magic, VNODE_MAGIC_DEAD);
    KTEST_ASSERT_EQ(vnode_get_destroy_cnt(), 1);
    KTEST_ASSERT(!node->deleted);

cleanup:
    if (vnode) { heap_free(vnode); }
    SMPSuiteVNode_test_cleanup(testctx);
}

typedef struct {
    vnode_t *exp_vnode;
    const char *path;
} SMPSuiteVNode_ResolveJob_arg_t;

KTEST_SMPJOB(ResolveJob) {
    const SMPSuiteVNode_ResolveJob_arg_t *const st_arg = arg;
    vnode_t *resolved_vnode = NULL;
    const char *const path_str = st_arg->path;

    for (size_t idx = 0; idx < 10; idx++) {
        const vpath_err_t resolve_err =
            vnode_resolve_path_str(path_str, &resolved_vnode);
        KTEST_ASSERT_EQ(resolve_err, VPATH_ERR_NONE);
        KTEST_ASSERT_EQ(resolved_vnode, st_arg->exp_vnode);
        KTEST_ASSERT_EQ(resolved_vnode->refcount, 1);
    }

cleanup:
    if (resolved_vnode) { heap_free(resolved_vnode); }
}

KTEST(SMPSuiteVNode, ConcurrentResolve) {
    SMPSuiteVNode_ctx_t *const ctx = &g_SMPSuiteVNode_ctx;
    vnode_t *dir_vnode = NULL;
    vnode_t *file_vnode = NULL;
    SMPSuiteVNode_ResolveJob_arg_t *arg = NULL;

    KTEST_PCALL(SMPSuiteVNode_test_setup);

    // Create vnode at `/dir/file`.
    const vfs_err_t mkdir_err =
        ctx->root->ops->f_mknode(ctx->root, &dir_vnode, "dir", VNODE_DIR);
    KTEST_ASSERT_EQ(mkdir_err, VFS_ERR_NONE);
    KTEST_ASSERT_NE(dir_vnode, NULL);
    const vfs_err_t mkfile_err =
        ctx->root->ops->f_mknode(dir_vnode, &file_vnode, "file", VNODE_FILE);
    KTEST_ASSERT_EQ(mkfile_err, VFS_ERR_NONE);
    KTEST_ASSERT_NE(file_vnode, NULL);

    arg = heap_alloc(sizeof(*arg));
    arg->exp_vnode = file_vnode;
    arg->path = "/dir/file";
    ktest_smpjob_broadcast(&KTEST_SMPJOB_REF(ResolveJob), arg);
    KTEST_PCALL(ktest_smpjob_wait, &KTEST_SMPJOB_REF(ResolveJob));

cleanup:
    heap_free(arg);
    heap_free(file_vnode);
    heap_free(dir_vnode);
    SMPSuiteVNode_test_cleanup(testctx);
}
