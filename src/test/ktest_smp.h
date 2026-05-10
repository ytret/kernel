#pragma once

#include <stddef.h>

typedef void (*ktest_smpfun_t)(void *arg);

typedef struct {
    volatile _Atomic size_t arrived;
    size_t target;
} ktest_smpbar_t;

typedef struct {
    const char *name;
    ktest_smpbar_t barrier;
    ktest_smpfun_t fn;
    void *fn_arg;
} ktest_smpjob_t;

void ktest_smpbar_init(ktest_smpbar_t *barrier, size_t target);
void ktest_smpbar_wait(const ktest_smpbar_t *barrier);

void ktest_smpjob_broadcast(ktest_smpjob_t *job);
void ktest_smpjob_wait(const ktest_smpjob_t *job);

void ktest_smpjob_do_local_job(void);
