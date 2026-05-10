#include "test/ktest.h"

KTEST_SUITE(KTEST_EARLY_BOOT, DummySuite);

KTEST(DummySuite, DummyTest) {
    testctx->failed = false;
    testctx->msg = "Hello, World!";
}
