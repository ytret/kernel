#include "cmdline.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "test/ktest.h"
#include "test/vmctl.h"

extern const ktest_suite_t ld_ktest_suites_start[];
extern const ktest_suite_t ld_ktest_suites_end[];

extern const ktest_test_t ld_ktest_tests_start[];
extern const ktest_test_t ld_ktest_tests_end[];

static bool prv_ktest_should_run_suite(const ktest_suite_t *suite);
static bool prv_ktest_should_run_test(const ktest_test_t *test);
static void prv_ktest_run_test(const ktest_test_t *test, ktest_testctx_t *ctx);

void ktest_run_stage(ktest_stage_t stage) {
    ktest_testctx_t testctx;

    for (const ktest_suite_t *suite = ld_ktest_suites_start;
         suite < ld_ktest_suites_end; suite++) {
        if (suite->stage != stage) { continue; }
        if (!prv_ktest_should_run_suite(suite)) { continue; }

        for (const ktest_test_t *test = ld_ktest_tests_start;
             test < ld_ktest_tests_end; test++) {
            if (!string_equals(test->suite_name, suite->name)) { continue; }
            if (!prv_ktest_should_run_test(test)) { continue; }

            kmemset(&testctx, 0, sizeof(testctx));

            prv_ktest_run_test(test, &testctx);

            if (testctx.failed) {
                LOG_ERROR("test %s.%s FAILED", test->suite_name,
                          test->test_name);
                LOG_ERROR("test is defined at %s line %d", test->file,
                          test->line);
                LOG_ERROR("test message: %s", testctx.msg);
                goto exit;
            } else {
                LOG_INFO("test %s.%s OK", test->suite_name, test->test_name);
            }
        }
    }

exit:
    vmctl_exit(0);
}

bool ktest_should_exit_at_end(int *exitcode) {
    if (exitcode) { *exitcode = 0; }
    return true;
}

static bool prv_ktest_should_run_suite(const ktest_suite_t *suite) {
    (void)suite;
    LOG_FLOW("check suite '%s' stage %d", suite->name, suite->stage);

    if (cmdline_num_values("ktest.run-all-suites") > 0) { return true; }

    const char *key = "ktest.run-suite";
    const size_t num_run_values = cmdline_num_values(key);
    char suite_name[KTEST_SUITE_NAME_MAX_SIZE];

    for (size_t idx = 0; idx < num_run_values; idx++) {
        cmdline_get_value(key, idx, suite_name, sizeof(suite_name));
        if (string_equals(suite_name, suite->name)) { return true; }
    }

    return false;
}

static bool prv_ktest_should_run_test(const ktest_test_t *test) {
    (void)test;
    LOG_FLOW("check test '%s' from suite '%s'", test->test_name,
             test->suite_name);
    return true;
}

static void prv_ktest_run_test(const ktest_test_t *test, ktest_testctx_t *ctx) {
    LOG_DEBUG("run test %s.%s", test->suite_name, test->test_name);
    test->fn(ctx);
}
