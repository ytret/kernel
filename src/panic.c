#include <stdarg.h>
#include <stdbool.h>

#include "acpi/lapic.h"
#include "config.h"
#include "kinttypes.h"
#include "kprintf.h"
#include "kspinlock.h"
#include "log.h"
#include "panic.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"

#define PANIC_MSG_SIZE             128
#define PANIC_STACKTRACE_MAX_ITEMS 32

static char g_panic_msg[PANIC_MSG_SIZE];
static uint32_t g_panic_stacktrace[PANIC_STACKTRACE_MAX_ITEMS];

static volatile bool g_panic_flag;
static spinlock_t g_panic_flag_lock;

// See panic.s.
extern int panic_walk_stack(uint32_t *arr_addr, uint32_t max_items);

static void prv_panic_enter(void);
static void prv_panic_set_flag(void);
static bool prv_panic_get_flag(void);
static void prv_panic_check_flag(void);
static void prv_panic_send_ipi(void);
[[maybe_unused]]
static void prv_panic_log_stacktrace(void);

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

#ifdef YTKERNEL_STACKTRACE_ON_PANIC
    prv_panic_log_stacktrace();
#endif

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

static void prv_panic_log_stacktrace(void) {
    LOG_ERROR("stacktrace:");

    const int num_items =
        panic_walk_stack(g_panic_stacktrace, PANIC_STACKTRACE_MAX_ITEMS);

    if (num_items > PANIC_STACKTRACE_MAX_ITEMS) {
        LOG_ERROR("panic_walk_stack placed too many items (%d > %d)", num_items,
                  PANIC_STACKTRACE_MAX_ITEMS);
    }

    if (num_items == 0) { LOG_ERROR("no entries"); }
    for (int idx = 0; idx < num_items; idx++) {
        const uint32_t addr = g_panic_stacktrace[idx];
        LOG_ERROR("%2d. 0x%08" PRIx32, idx + 1, addr);
    }

    LOG_ERROR("further entries may be unmapped");
}
