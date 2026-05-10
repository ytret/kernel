#pragma once

#include <stddef.h>

#include "test/ktest.h"

typedef void (*ktest_smpjob_fn_t)(ktest_testctx_t *test, void *arg);

typedef struct {
    volatile _Atomic size_t arrived;
    size_t target;
} ktest_smpbar_t;

/**
 * Global SMP test job descriptor and context.
 */
typedef struct {
    const char *name;
    const ktest_smpjob_fn_t fn;

    ktest_smpbar_t barrier;
} ktest_smpjob_t;

/**
 * Processor-local SMP test job context.
 */
typedef struct {
    bool done;
    ktest_smpjob_t *job;
    void *fn_arg;
    ktest_testctx_t *testctx;
} ktest_smpjob_local_t;

void ktest_smpbar_init(ktest_smpbar_t *barrier, size_t target);
void ktest_smpbar_wait(const ktest_smpbar_t *barrier);

void ktest_smpjob_broadcast(ktest_smpjob_t *job, void *fn_arg);
void ktest_smpjob_wait(const ktest_smpjob_t *job);

void ktest_smpjob_do_local_job(void);
