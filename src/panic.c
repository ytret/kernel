#include <stdbool.h>

#include "acpi/lapic.h"
#include "kprintf.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"

static volatile bool b_in_panic;

static void prv_panic_send_ipi(void);

void panic_enter(void) {
    prv_panic_send_ipi();
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

static void prv_panic_send_ipi(void) {
    if (!smp_is_active()) { return; }
    const lapic_icr_t ipi_halt = {
        .vector = SMP_VEC_HALT,
        .delmod = LAPIC_ICR_DELMOD_FIXED,
        .destmod = APIC_DESTMOD_PHYSICAL, // ignored because destsh is used
        .level = LAPIC_ICR_ASSERT, // must be ASSERT because it's not INIT
        .trigmod = 0,              // ignored because it's not INIT
        .destsh = LAPIC_ICR_DEST_ALL_BUT_SELF,
        .dest = 0, // ignored because destsh is used
    };
    lapic_send_ipi(&ipi_halt);
}
