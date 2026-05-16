#include "test/earlyboot/earlyboot_tests.h"
#include "test/ktests.h"

static ktest_suite_t g_ktest_suite_dummy = {
    .name = "Dummy",
};

KTEST_SUITE(Dummy);

static void earlyboot_test_dummy_init(list_t *suites);

void tests_earlyboot_init(list_t *suites) {
    earlyboot_test_dummy_init(suites);
}

KTEST(Dummy, Dummy) {
    KTEST_FAIL(1, "%s", "eh");
}

static void earlyboot_test_dummy_init(list_t *suites) {
    list_append(suites, &g_ktest_suite_dummy.list_node);
}
