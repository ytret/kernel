#include "heap.h"
#include "log.h"
#include "memfun.h"
#include "smp.h"
#include "test/ktest_smp.h"

void ktest_smpbar_init(ktest_smpbar_t *barrier, size_t target) {
    barrier->arrived = 0;
    barrier->target = target;
}

void ktest_smpbar_wait(const ktest_smpbar_t *barrier) {
    while (barrier->arrived != barrier->target) {
        __asm__ volatile("pause" ::: "memory");
    }
}

void ktest_smpjob_broadcast(ktest_smpjob_t *job, void *fn_arg) {
    const uint8_t num_procs = smp_get_num_procs();

    ktest_smpbar_init(&job->barrier, num_procs);

    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        smp_proc_t *const proc = smp_get_proc(proc_num);
        spinlock_acquire(&proc->ktest_lock);
        proc->ktest_job.done = false;
        proc->ktest_job.job = job;
        proc->ktest_job.fn_arg = fn_arg;
        proc->ktest_job.testctx = heap_alloc(sizeof(ktest_testctx_t));
        kmemset(proc->ktest_job.testctx, 0, sizeof(ktest_testctx_t));
        proc->ktest_job.testctx->failed = false;
        spinlock_release(&proc->ktest_lock);
    }

    ktest_smpjob_do_local_job();
}

void ktest_smpjob_wait(ktest_testctx_t *testctx, const ktest_smpjob_t *job) {
    ktest_smpbar_wait(&job->barrier);

    size_t num_failed = 0;

    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        const smp_proc_t *const proc = smp_get_proc(proc_num);
        if (proc->ktest_job.testctx->failed) {
            LOG_ERROR("proc %u job %s failed", proc_num,
                      proc->ktest_job.job->name);
            LOG_ERROR("proc %u job %s message: %s", proc_num,
                      proc->ktest_job.job->name, proc->ktest_job.testctx->msg);
            num_failed++;
        }
    }

    if (num_failed > 0) {
        testctx->failed = true;
        ksnprintf(testctx->msg, sizeof(testctx->msg), "%zu SMP jobs failed",
                  num_failed);
    }
}

void ktest_smpjob_do_local_job(void) {
    smp_proc_t *const proc = smp_get_running_proc();

    spinlock_acquire(&proc->ktest_lock);
    if (proc->ktest_job.job && !proc->ktest_job.done) {
        LOG_FLOW("proc num %u, do job %s", proc->proc_num,
                 proc->ktest_job.job->name);

        proc->ktest_job.job->fn(proc->ktest_job.testctx,
                                proc->ktest_job.fn_arg);
        proc->ktest_job.job->barrier.arrived++;

        proc->ktest_job.done = true;
    }
    spinlock_release(&proc->ktest_lock);
}
