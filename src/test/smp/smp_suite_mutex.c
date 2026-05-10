#include <stdatomic.h>
#include <stddef.h>

#include "heap.h"
#include "kmutex.h"
#include "memfun.h"
#include "smp.h"
#include "test/ktest.h"
#include "test/ktest_smp.h"

typedef struct {
    uint8_t num_procs;
} SMPSuiteMutex_ctx_t;

typedef struct {
    task_mutex_t mutex;
    ktest_smpbar_t start_barrier;
    size_t num_iters;

    _Atomic size_t inside_cnt;
    size_t protected_counter;
} SMPSuiteMutex_Contention_arg_t;

typedef struct {
    task_mutex_t mutex;
    ktest_smpbar_t start_barrier;

    atomic_bool owner_locked;
    _Atomic uint8_t waiters_ready;
    _Atomic uint8_t acquired_cnt;
    _Atomic uint8_t inside_cnt;
} SMPSuiteMutex_Handoff_arg_t;

static SMPSuiteMutex_ctx_t g_SMPSuiteMutex_ctx;

static void SMPSuiteMutex_test_setup(ktest_testctx_t *testctx);
static void SMPSuiteMutex_test_cleanup(ktest_testctx_t *testctx);

static void prv_smpbar_arrive_and_wait(ktest_smpbar_t *barrier);

KTEST_SUITE(KTEST_SMP, SMPSuiteMutex);

static void SMPSuiteMutex_test_setup(ktest_testctx_t *testctx) {
    SMPSuiteMutex_ctx_t *const ctx = &g_SMPSuiteMutex_ctx;

    ctx->num_procs = smp_get_num_procs();
    KTEST_ASSERT(ctx->num_procs > 1);

cleanup:
    (void)testctx;
}

static void SMPSuiteMutex_test_cleanup(ktest_testctx_t *testctx) {
    (void)testctx;
}

KTEST_SMPJOB(ContentionJob) {
    SMPSuiteMutex_Contention_arg_t *const st_arg = arg;
    bool lock_held = false;

    prv_smpbar_arrive_and_wait(&st_arg->start_barrier);

    for (size_t idx = 0; idx < st_arg->num_iters; idx++) {
        mutex_acquire(&st_arg->mutex);
        lock_held = true;
        KTEST_ASSERT(mutex_caller_owns(&st_arg->mutex));

        const size_t prev_inside = st_arg->inside_cnt++;
        KTEST_ASSERT_EQ(prev_inside, 0);

        const size_t prev_counter = st_arg->protected_counter;
        // TODO: delay
        // prv_pause_loop(32);
        st_arg->protected_counter = prev_counter + 1;

        const size_t prev_after = st_arg->inside_cnt--;
        KTEST_ASSERT_EQ(prev_after, 1);

        mutex_release(&st_arg->mutex);
        lock_held = false;
    }

cleanup:
    if (lock_held) { mutex_release(&st_arg->mutex); }
    return;
}

KTEST(SMPSuiteMutex, Contention) {
    SMPSuiteMutex_ctx_t *const ctx = &g_SMPSuiteMutex_ctx;
    SMPSuiteMutex_Contention_arg_t *arg = NULL;

    KTEST_PCALL(SMPSuiteMutex_test_setup);

    arg = heap_alloc(sizeof(*arg));
    kmemset(arg, 0, sizeof(*arg));

    mutex_init(&arg->mutex);
    ktest_smpbar_init(&arg->start_barrier, ctx->num_procs);
    arg->num_iters = 100;

    ktest_smpjob_broadcast(&KTEST_SMPJOB_REF(ContentionJob), arg);
    KTEST_PCALL(ktest_smpjob_wait, &KTEST_SMPJOB_REF(ContentionJob));

    KTEST_ASSERT_EQ(arg->inside_cnt, 0);
    KTEST_ASSERT_EQ(arg->protected_counter, ctx->num_procs * arg->num_iters);
    KTEST_ASSERT_EQ(arg->mutex.locking_task, NULL);

cleanup:
    if (arg) { heap_free(arg); }
    SMPSuiteMutex_test_cleanup(testctx);
}

KTEST_SMPJOB(HandoffJob) {
    SMPSuiteMutex_Handoff_arg_t *const st_arg = arg;
    const smp_proc_t *const proc = smp_get_running_proc();
    bool lock_held = false;

    prv_smpbar_arrive_and_wait(&st_arg->start_barrier);

    if (proc->proc_num == 0) {
        mutex_acquire(&st_arg->mutex);
        lock_held = true;
        KTEST_ASSERT(mutex_caller_owns(&st_arg->mutex));

        st_arg->owner_locked = true;

        while (st_arg->waiters_ready != smp_get_num_procs() - 1) {
            __asm__ volatile("pause" ::: "memory");
        }

        // Give the waiters time to enter the blocked acquire path.
        // TODO: delay
        // prv_pause_loop(256);

        mutex_release(&st_arg->mutex);
        lock_held = false;
    } else {
        while (!st_arg->owner_locked) {
            __asm__ volatile("pause" ::: "memory");
        }

        st_arg->waiters_ready++;
        mutex_acquire(&st_arg->mutex);
        lock_held = true;
        KTEST_ASSERT(mutex_caller_owns(&st_arg->mutex));

        const size_t prev_inside = st_arg->inside_cnt++;
        KTEST_ASSERT_EQ(prev_inside, 0);

        st_arg->acquired_cnt++;
        // TODO: delay
        // prv_pause_loop(32);

        const size_t prev_after = st_arg->inside_cnt--;
        KTEST_ASSERT_EQ(prev_after, 1);

        mutex_release(&st_arg->mutex);
        lock_held = false;
    }

cleanup:
    if (lock_held) { mutex_release(&st_arg->mutex); }
    return;
}

KTEST(SMPSuiteMutex, Handoff) {
    SMPSuiteMutex_ctx_t *const ctx = &g_SMPSuiteMutex_ctx;
    SMPSuiteMutex_Handoff_arg_t *arg = NULL;

    KTEST_PCALL(SMPSuiteMutex_test_setup);

    arg = heap_alloc(sizeof(*arg));
    kmemset(arg, 0, sizeof(*arg));

    mutex_init(&arg->mutex);
    ktest_smpbar_init(&arg->start_barrier, ctx->num_procs);

    ktest_smpjob_broadcast(&KTEST_SMPJOB_REF(HandoffJob), arg);
    KTEST_PCALL(ktest_smpjob_wait, &KTEST_SMPJOB_REF(HandoffJob));

    KTEST_ASSERT_EQ(arg->inside_cnt, 0);
    KTEST_ASSERT_EQ(arg->acquired_cnt, ctx->num_procs - 1);
    KTEST_ASSERT_EQ(arg->mutex.locking_task, NULL);

cleanup:
    if (arg) { heap_free(arg); }
    SMPSuiteMutex_test_cleanup(testctx);
}

static void prv_smpbar_arrive_and_wait(ktest_smpbar_t *barrier) {
    barrier->arrived++;
    ktest_smpbar_wait(barrier);
}
