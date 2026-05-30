#include "cmdline.h"
#include "kstring.h"
#include "log.h"
#include "memfun.h"
#include "test/ktest.h"
#include "test/vmctl.h"

static ktest_globalctx_t g_ktest_globalctx;

extern const ktest_suite_t ld_ktest_suites_start[];
extern const ktest_suite_t ld_ktest_suites_end[];

extern const ktest_test_t ld_ktest_tests_start[];
extern const ktest_test_t ld_ktest_tests_end[];

static void prv_ktest_tap_version(void);
static void prv_ktest_tap_plan(void);
static void prv_ktest_tap_testpoint(bool ok, uint32_t number, const char *desc);
static void prv_ktest_puts(const char *str);

static bool prv_ktest_should_run_suite(const ktest_suite_t *suite);
static bool prv_ktest_should_run_test(const ktest_test_t *test);
static bool prv_ktest_should_exit_vm(void);
static void prv_ktest_run_test(const ktest_test_t *test, ktest_testctx_t *ctx);

void ktest_set_chardev(chardev_t *chardev) {
    g_ktest_globalctx.chardev = chardev;
}

void ktest_announce(void) {
    prv_ktest_tap_version();
    prv_ktest_tap_plan();
}

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
            } else {
                LOG_INFO("test %s.%s OK", test->suite_name, test->test_name);
            }
        }
    }
}

void ktest_end(void) {
    int ktest_exitcode = 0;

    const ktest_globalctx_t *const ktest_ctx = ktest_get_globalctx();
    const int ratio_x1000 =
        ktest_ctx->tests_run == 0
            ? 0
            : ktest_ctx->tests_passed * 1000 / ktest_ctx->tests_run;

    LOG_INFO("ktest results:");
    LOG_INFO("%zu.%zu%% tests passed, %zu failed out of %zu",
             ratio_x1000 / (size_t)10, ratio_x1000 % (size_t)10,
             ktest_ctx->tests_failed, ktest_ctx->tests_run);

    if (prv_ktest_should_exit_vm()) { vmctl_exit(ktest_exitcode); }
}

ktest_globalctx_t *ktest_get_globalctx(void) {
    return &g_ktest_globalctx;
}

static void prv_ktest_tap_version(void) {
    prv_ktest_puts("TAP version 14\n");
}

static void prv_ktest_tap_plan(void) {
    // TODO
}

static void prv_ktest_tap_testpoint(bool ok, uint32_t number,
                                    const char *desc) {
    char *const buf = g_ktest_globalctx.tap_msg;
    size_t buf_off = 0;
    size_t left = KTEST_TAP_MSG_MAX_SIZE;

    buf_off += klsnprintf(buf, left, "%s", ok ? "ok" : "not ok");
    left = KTEST_TAP_MSG_MAX_SIZE - buf_off;

    if (number > 0) {
        buf_off += klsnprintf(&buf[buf_off], left, " %u", number);
        left = KTEST_TAP_MSG_MAX_SIZE - buf_off;
    }

    if (desc) {
        buf_off += klsnprintf(&buf[buf_off], left, " - %s", desc);
        left = KTEST_TAP_MSG_MAX_SIZE - buf_off;
    }

    if (left == 0) {
        buf[buf_off - 1] = '\0';
    } else {
        buf[buf_off] = '\n';
        buf[buf_off + 1] = '\0';
    }

    prv_ktest_puts(buf);
}

static void prv_ktest_puts(const char *str) {
    chardev_t *const chardev = g_ktest_globalctx.chardev;
    if (!chardev) { return; }
    if (chardev->type == CHARDEV_UNINIT) { return; }

    const size_t len = string_len(str);
    chardev->ops->f_write(chardev->ctx, str, len, NULL);
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

static bool prv_ktest_should_exit_vm(void) {
    return cmdline_num_values("ktest.exit-vm");
}

static void prv_ktest_run_test(const ktest_test_t *test,
                               ktest_testctx_t *testctx) {
    LOG_DEBUG("run test %s.%s", test->suite_name, test->test_name);
    ktest_globalctx_t *const globalctx = &g_ktest_globalctx;

    test->fn(testctx);

    globalctx->tests_run++;
    if (testctx->failed) {
        globalctx->tests_failed++;
    } else {
        globalctx->tests_passed++;
    }

    const int desc_len = ksnprintf(globalctx->tap_desc, KTEST_TAP_DESC_MAX_SIZE,
                                   "%s.%s", test->suite_name, test->test_name);
    if (desc_len == KTEST_TAP_DESC_MAX_SIZE) {
        globalctx->tap_desc[desc_len - 1] = '\0';
    }
    prv_ktest_tap_testpoint(!testctx->failed, globalctx->tests_run,
                            globalctx->tap_desc);
}
