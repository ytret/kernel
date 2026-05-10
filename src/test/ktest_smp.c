#include "log.h"
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

void ktest_smpjob_broadcast(ktest_smpjob_t *job) {
    const uint8_t num_procs = smp_get_num_procs();

    ktest_smpbar_init(&job->barrier, num_procs);

    for (uint8_t proc_num = 0; proc_num < smp_get_num_procs(); proc_num++) {
        smp_proc_t *const proc = smp_get_proc(proc_num);
        spinlock_acquire(&proc->ktest_lock);
        proc->ktest_job = job;
        proc->ktest_job_done = false;
        spinlock_release(&proc->ktest_lock);
    }

    ktest_smpjob_do_local_job();
}

void ktest_smpjob_wait(const ktest_smpjob_t *job) {
    ktest_smpbar_wait(&job->barrier);
}

void ktest_smpjob_do_local_job(void) {
    smp_proc_t *const proc = smp_get_running_proc();

    spinlock_acquire(&proc->ktest_lock);
    if (proc->ktest_job && !proc->ktest_job_done) {
        LOG_FLOW("proc num %u, do job %s", proc->proc_num,
                 proc->ktest_job->name);
        proc->ktest_job->fn(proc->ktest_job->fn_arg);
        proc->ktest_job->barrier.arrived++;
        proc->ktest_job_done = true;
    }
    spinlock_release(&proc->ktest_lock);
}
