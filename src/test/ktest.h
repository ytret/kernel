#pragma once

#include <stddef.h>

#define KTEST_SUITE_NAME_MAX_SIZE 32
#define KTEST_TEST_NAME_MAX_SIZE  64

#define KTEST_SUITE(Stage, SuiteName)                                          \
    static const ktest_suite_t ktest_suite_##SuiteName                         \
        __attribute__((used, section(".ktest_suites"),                         \
                       aligned(__alignof__(ktest_suite_t)))) = {               \
            .name = #SuiteName,                                                \
            .stage = Stage,                                                    \
    };

#define KTEST(SuiteName, TestName)                                             \
    static void ktest_fn_##SuiteName##_##TestName(ktest_testctx_t *testctx);   \
    static const ktest_test_t ktest_##SuiteName##_##TestName                   \
        __attribute__((used, section(".ktest_tests"),                          \
                       aligned(__alignof__(ktest_test_t)))) = {                \
            .suite_name = #SuiteName,                                          \
            .test_name = #TestName,                                            \
            .file = __FILE_NAME__,                                             \
            .line = __LINE__,                                                  \
            .fn = ktest_fn_##SuiteName##_##TestName,                           \
    };                                                                         \
    static void ktest_fn_##SuiteName##_##TestName(__attribute__((unused))      \
                                                  ktest_testctx_t *testctx)

typedef enum {
    KTEST_EARLY_BOOT,
    KTEST_PRE_SMP,
    KTEST_SMP,
} ktest_stage_t;

typedef struct {
    const char name[KTEST_SUITE_NAME_MAX_SIZE];
    ktest_stage_t stage;
} ktest_suite_t;

typedef struct {
    bool failed;
    char *msg;
} ktest_testctx_t;

typedef struct {
    const char suite_name[KTEST_SUITE_NAME_MAX_SIZE];
    const char test_name[KTEST_TEST_NAME_MAX_SIZE];
    const char *file;
    const int line;
    void (*const fn)(ktest_testctx_t *testctx);
} ktest_test_t;

typedef struct {
    size_t tests_run;
    size_t tests_passed;
    size_t tests_failed;

    bool abort_requested;
} ktest_globalctx_t;

void ktest_run_stage(ktest_stage_t stage);
bool ktest_should_exit_at_end(int *exitcode);
ktest_globalctx_t *ktest_get_globalctx(void);
