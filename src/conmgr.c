#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "assert.h"
#include "conmgr.h"
#include "console.h"
#include "dynarr.h"
#include "inputmgr.h"
#include "kspinlock.h"
#include "log.h"
#include "tty.h"

typedef struct {
    spinlock_t lock;

    dynarr_t cons;
    console_t *active_con;

    dynarr_t ttys;
    tty_t *active_tty;

    textdisp_t *disp;
} conmgr_t;

static conmgr_t g_conmgr;

void conmgr_init(size_t num_add_cons) {
    spinlock_init(&g_conmgr.lock);

    textdisp_t *const boot_disp = textdisp_get_boot_disp();

    dynarr_init(&g_conmgr.cons, sizeof(console_t *), _Alignof(console_t *),
                1 + num_add_cons);
    console_t *const boot_con = console_get_boot_con();
    ASSERT(dynarr_push(&g_conmgr.cons, &boot_con, NULL));

    dynarr_init(&g_conmgr.ttys, sizeof(tty_t *), _Alignof(tty_t *),
                1 + num_add_cons);
    tty_t *const boot_tty = tty_get_boot_tty();
    ASSERT(dynarr_push(&g_conmgr.ttys, &boot_tty, NULL));

    g_conmgr.active_con = boot_con;
    g_conmgr.active_tty = boot_tty;
    g_conmgr.disp = boot_disp;

    for (size_t i = 0; i < num_add_cons; i++) {
        console_t *const con = console_new();
        tty_t *const tty = tty_new();
        tty_set_out(tty, console_chardev(con));

        ASSERT(dynarr_push(&g_conmgr.cons, &con, NULL));
        ASSERT(dynarr_push(&g_conmgr.ttys, &tty, NULL));
    }
}

bool conmgr_switch(size_t idx) {
    LOG_FLOW("idx %zu", idx);

    spinlock_acquire(&g_conmgr.lock);

    if (idx >= g_conmgr.cons.num_items) {
        LOG_ERROR("console %zu does not exist", idx);
        spinlock_release(&g_conmgr.lock);
        return false;
    }

    DEBUG_ASSERT(g_conmgr.disp != NULL);

    console_t *sel_con;
    ASSERT(dynarr_get_at(&g_conmgr.cons, idx, &sel_con, sizeof(console_t *)));
    DEBUG_ASSERT(sel_con != NULL);

    tty_t *sel_tty;
    ASSERT(dynarr_get_at(&g_conmgr.ttys, idx, &sel_tty, sizeof(tty_t *)));
    DEBUG_ASSERT(sel_tty != NULL);

    if (g_conmgr.active_con) {
        console_lock(g_conmgr.active_con);
        textdisp_t *const disp = console_detach(g_conmgr.active_con);
        console_unlock(g_conmgr.active_con);
        ASSERT(disp == g_conmgr.disp);
    }

    console_lock(sel_con);
    const bool attached = console_attach(sel_con, g_conmgr.disp);
    console_unlock(sel_con);

    tty_lock(sel_tty);
    inputmgr_set_tty(sel_tty);
    tty_unlock(sel_tty);

    if (attached) { g_conmgr.active_con = sel_con; }

    spinlock_release(&g_conmgr.lock);
    return attached;
}
