#pragma once

#include "list.h"
#include "log.h"

#define KTEST_FAIL(exitcode, fmt, ...)                                         \
    LOG_ERROR("test %s failed", __func__);                                     \
    LOG_ERROR(fmt, ##__VA_ARGS__);                                             \
    return exitcode;
#define KTEST_TRUE(cond)                                                       \
    if (!(cond)) { return 1; }

#define _KTEST_STR_INNER(x) #x
#define _KTEST_STR(x)       _KTEST_STR_INNER(x)

#define KTEST_SUITE(SuiteName) static ktest_suite_t g_ktest_suite_dummy

#define KTEST(SuiteName, TestName)                                             \
    static int test_##SuiteName##_##TestName(void);                            \
    static ktest_test_t g_test_##SuiteName##_##TestName = {                    \
        .name = "" _KTEST_STR(SuiteName) "",                                   \
        .f_test = test_##SuiteName##_##TestName,                               \
    };                                                                         \
    static int test_##SuiteName##_##TestName(void)

typedef enum {
    KTEST_EARLY_BOOT,
} ktest_stage_t;

typedef struct {
    list_node_t list_node;
    const char *name;
    int (*f_test)(void);
} ktest_test_t;

typedef struct {
    list_node_t list_node;
    const char *name;
    list_t tests; // item type: ktest_test_t
} ktest_suite_t;

void ktests_init(void);
int ktests_run(ktest_stage_t stage);
