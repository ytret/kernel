#include <stdarg.h>
#include <stdbool.h>

#include "acpi/lapic.h"
#include "kprintf.h"
#include "kspinlock.h"
#include "log.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"

#define PANIC_MSG_SIZE 128

static char g_panic_msg[PANIC_MSG_SIZE];

static volatile bool g_panic_flag;
static spinlock_t g_panic_flag_lock;

static void prv_panic_enter(void);
static void prv_panic_set_flag(void);
static bool prv_panic_get_flag(void);
static void prv_panic_check_flag(void);

static void prv_panic_send_ipi(void);

[[gnu::noreturn]]
void panic_helper(const char *file, const char *func, int line, const char *fmt,
                  ...) {
    va_list ap;

    prv_panic_enter();

    va_start(ap, fmt);
    kvsnprintf(g_panic_msg, PANIC_MSG_SIZE, fmt, ap);
    va_end(ap);

    term_enter_panic_mode();
    LOG_ERROR("%s", "");
    LOG_ERROR("==== KERNEL PANIC ====");
    LOG_ERROR(" Message: %s", g_panic_msg);
    LOG_ERROR("    File: %s", file);
    LOG_ERROR("Function: %s", func);
    LOG_ERROR("    Line: %d", line);

    for (;;) {
        __asm__ volatile("hlt");
    }
}

[[gnu::noreturn]] void panic_nomsg(void) {
    prv_panic_enter();

    for (;;) {
        __asm__ volatile("hlt");
    }
}

[[gnu::noreturn]]
void panic_nested(void) {
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void prv_panic_enter(void) {
    prv_panic_check_flag();
    prv_panic_set_flag();

    prv_panic_send_ipi();
    __asm__ volatile("cli");

    if (smp_get_running_taskmgr()) { taskmgr_local_lock_scheduler(); }
}

static void prv_panic_set_flag(void) {
    spinlock_acquire(&g_panic_flag_lock);
    g_panic_flag = true;
    spinlock_release(&g_panic_flag_lock);
}

static bool prv_panic_get_flag(void) {
    bool ret;

    spinlock_acquire(&g_panic_flag_lock);
    ret = g_panic_flag;
    spinlock_release(&g_panic_flag_lock);

    return ret;
}

static void prv_panic_check_flag(void) {
    if (prv_panic_get_flag()) { panic_nested(); }
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
