#include "assert.h"
#include "list.h"
#include "test/earlyboot/earlyboot_tests.h"
#include "test/ktests.h"

typedef struct {
    list_t early_boot_suites;
} ktests_t;

static ktests_t g_ktests;

static int prv_ktests_run_suites(const list_t *suite_list);
static int prv_ktests_run_suite(const ktest_suite_t *suite);

void ktests_init(void) {
    tests_earlyboot_init(&g_ktests.early_boot_suites);
}

int ktests_run(ktest_stage_t stage) {
    list_t *run_list;
    bool stage_ok = false;
    switch (stage) {
    case KTEST_EARLY_BOOT:
        stage_ok = true;
        run_list = &g_ktests.early_boot_suites;
        break;
    }
    ASSERT(stage_ok);

    return prv_ktests_run_suites(run_list);
}

static int prv_ktests_run_suites(const list_t *suite_list) {
    for (const list_node_t *node = suite_list->p_first_node; node != NULL;
         node = node->p_next) {
        const ktest_suite_t *const suite =
            LIST_NODE_TO_STRUCT(node, ktest_suite_t, list_node);
        const int ec = prv_ktests_run_suite(suite);
        if (ec != 0) { return ec; }
    }
    return 0;
}

static int prv_ktests_run_suite(const ktest_suite_t *suite) {
    for (const list_node_t *node = suite->tests.p_first_node; node != NULL;
         node = node->p_next) {
        const ktest_test_t *const test =
            LIST_NODE_TO_STRUCT(node, ktest_test_t, list_node);
        ASSERT(test->f_test);
        const int ec = test->f_test();
        if (ec != 0) { return ec; }
    }
    return 0;
}
