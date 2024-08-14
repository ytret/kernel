#include <stdbool.h>

#include "kprintf.h"
#include "panic.h"
#include "taskmgr.h"

static volatile bool b_in_panic;

__attribute__((noreturn)) void panic(char const *p_msg) {
    taskmgr_lock_scheduler();

    if (b_in_panic) { panic_silent(); }

    b_in_panic = true;
    kprintf("Kernel panic: %s. Halting.\n", p_msg);

    for (;;) {}
}

__attribute__((noreturn)) void panic_silent(void) {
    taskmgr_lock_scheduler();

    for (;;) {}
}
