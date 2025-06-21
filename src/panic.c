#include <stdbool.h>

#include "kprintf.h"
#include "panic.h"
#include "taskmgr.h"
#include "term.h"

static volatile bool b_in_panic;

void panic_enter(void) {
    taskmgr_lock_scheduler();
    term_enter_panic_mode();

    term_print_str("\n");
    kprintf("==== KERNEL PANIC ====\n");
}

[[gnu::noreturn]]
void panic(char const *p_msg) {
    if (b_in_panic) { panic_silent(); }

    b_in_panic = true;
    kprintf("Kernel panic: %s. Halting.\n", p_msg);

    for (;;) {}
}

[[gnu::noreturn]]
void panic_silent(void) {
    taskmgr_lock_scheduler();
    for (;;) {}
}
