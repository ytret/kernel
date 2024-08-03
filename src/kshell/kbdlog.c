#include "kprintf.h"
#include "kshell/kbdlog.h"
#include "kshell/kshell.h"

#define EVENT_BUF_LEN 20

typedef struct {
    unsigned char key;
    bool b_released;
} event_t;

static event_t gp_events[EVENT_BUF_LEN];
static size_t g_num_events;
static volatile size_t g_events_pos;

static void add_event(unsigned char key, bool b_released);

void kbdlog(size_t num_events) {
    // Reset after a previous run.
    g_events_pos = 0;

    if (num_events <= EVENT_BUF_LEN) {
        g_num_events = num_events;
    } else {
        kprintf("kbdlog: requested number of events %u exceeds the maximum"
                " supported number of %u events\n",
                num_events, EVENT_BUF_LEN);
        g_num_events = EVENT_BUF_LEN;
    }
    kprintf("kbdlog: capturing %u events\n", g_num_events);

    kshell_set_kbd_handler(add_event);

    size_t idx = 0;
    while (g_events_pos < g_num_events) {
        if (idx < g_events_pos) {
            kprintf("%3u: key = %u, ", idx, gp_events[idx].key);
            if (gp_events[idx].b_released) {
                kprintf("released\n");
            } else {
                kprintf("pressed\n");
            }

            idx = g_events_pos;
        }
    }
}

static void add_event(unsigned char key, bool b_released) {
    if (g_events_pos < g_num_events) {
        gp_events[g_events_pos].key = key;
        gp_events[g_events_pos].b_released = b_released;
        g_events_pos++;
    }
}
