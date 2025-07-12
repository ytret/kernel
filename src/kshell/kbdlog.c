#include "heap.h"
#include "kbd.h"
#include "kprintf.h"
#include "kshell/kbdlog.h"
#include "term.h"

#define EVENT_BUF_LEN 20

void kbdlog(size_t num_events) {
    kprintf("kbdlog: capturing %u events\n", num_events);

    kbd_event_t event;
    for (size_t idx = 0; idx < num_events; idx++) {
        term_read_kbd_event(&event);

        kprintf("kbdlog: %3u: key = %u, ", idx, event.key);
        if (event.b_released) {
            kprintf("released\n");
        } else {
            kprintf("pressed\n");
        }
    }
}
