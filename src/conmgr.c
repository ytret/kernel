#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "assert.h"
#include "conmgr.h"
#include "console.h"
#include "dynarr.h"
#include "kbd.h"
#include "log.h"

typedef struct {
    dynarr_t cons;
    console_t *active_con;
    textdisp_t *disp;
} conmgr_t;

static conmgr_t g_conmgr;

void conmgr_init(size_t num_add_cons) {
    dynarr_init(&g_conmgr.cons, sizeof(console_t *), _Alignof(console_t *),
                1 + num_add_cons);

    textdisp_t *const boot_disp = textdisp_get_boot_disp();
    console_t *const boot_con = console_get_boot_con();
    ASSERT(dynarr_push(&g_conmgr.cons, &boot_con, NULL));

    g_conmgr.active_con = boot_con;
    g_conmgr.disp = boot_disp;

    for (size_t i = 0; i < num_add_cons; i++) {
        console_t *const con = console_new();
        ASSERT(dynarr_push(&g_conmgr.cons, &con, NULL));
    }
}

bool conmgr_switch(size_t idx) {
    LOG_FLOW("idx %zu", idx);

    if (idx >= g_conmgr.cons.num_items) {
        LOG_ERROR("console %zu does not exist", idx);
        return false;
    }

    DEBUG_ASSERT(g_conmgr.disp != NULL);

    console_t *sel_con;
    ASSERT(dynarr_get_at(&g_conmgr.cons, idx, &sel_con, sizeof(console_t *)));
    DEBUG_ASSERT(sel_con != NULL);

    if (g_conmgr.active_con) {
        console_lock(g_conmgr.active_con);
        textdisp_t *const disp = console_detach(g_conmgr.active_con);
        console_unlock(g_conmgr.active_con);
        ASSERT(disp == g_conmgr.disp);
    }

    console_lock(sel_con);
    const bool attached = console_attach(sel_con, g_conmgr.disp);
    console_unlock(sel_con);

    if (attached) {
        g_conmgr.active_con = sel_con;
        return true;
    } else {
        return false;
    }
}

[[gnu::noreturn]] void conmgr_task(void) {
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    __asm__ volatile("sti");

    kbd_event_t event;
    queue_t *const sysevents = kbd_sysevent_queue();
    for (;;) {
        queue_read(sysevents, &event, sizeof(event));

        if (event.key == KEY_1) { conmgr_switch(0); }
        if (event.key == KEY_2) { conmgr_switch(1); }
        if (event.key == KEY_3) { conmgr_switch(2); }
        if (event.key == KEY_4) { conmgr_switch(3); }
        if (event.key == KEY_5) { conmgr_switch(4); }
        if (event.key == KEY_6) { conmgr_switch(5); }
        if (event.key == KEY_7) { conmgr_switch(6); }
        if (event.key == KEY_8) { conmgr_switch(7); }
        if (event.key == KEY_9) { conmgr_switch(8); }
        if (event.key == KEY_0) { conmgr_switch(9); }
    }

    PANIC("end of conmgr task");
}
