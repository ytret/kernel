#include <stdbool.h>
#include <stdint.h>

#include "heap.h"
#include "kbd.h"
#include "kprintf.h"
#include "kshell/vasview.h"
#include "panic.h"
#include "term.h"

#define VIEW_START_ROW 2
#define VIEW_START_COL 8
#define VIEW_ROWS      16
#define VIEW_COLS      64

#define VIEW_DIR 0
#define VIEW_TBL 1

#define INFO_START_ROW (VIEW_START_ROW + VIEW_ROWS + 1)
#define INFO_ROWS      4

#define FLAG_PRESENT  (1 << 0)
#define FLAG_WRITABLE (1 << 1)
#define FLAG_ANY_DPL  (1 << 2)

static volatile bool gb_exit;

static volatile int g_view;
static uint32_t *gp_pgdir;

// Cursors on level 1 (dir) and level 2 (table).
static volatile size_t g_dir_idx;
static volatile size_t g_tbl_idx;

static void update_full(void);
static void update_view(void);
static void update_info(void);
static void update_cursor(void);

static void show_dir(void);
static void show_tbl(void);

static uint32_t entry_at_cursor(void);
static uint32_t idx_at_cursor(void);

static void parse_kbd_event(kbd_event_t *p_event);
static void move_cursor(int32_t inc_idx);
static void deeper_view(void);
static void shallower_view(void);

void vasview(uint32_t pgdir_virt) {
    gb_exit = false;
    gp_pgdir = ((uint32_t *)pgdir_virt);
    g_dir_idx = 0;
    g_tbl_idx = 0;

    term_acquire_mutex();
    update_full();
    term_release_mutex();

    while (!gb_exit) {
        kbd_event_t kbd_event;
        term_read_kbd_event(&kbd_event);
        parse_kbd_event(&kbd_event);
    }

    // Before returning to kshell, put the cursor on the last row, to make the
    // prompt appear there.
    term_acquire_mutex();
    term_put_cursor_at((term_height() - 1), 0);
    term_release_mutex();
}

static void update_full(void) {
    update_view();
    update_info();
    update_cursor();
}

static void update_view(void) {
    switch (g_view) {
    case VIEW_DIR:
        show_dir();
        break;
    case VIEW_TBL:
        show_tbl();
        break;

    default:
        panic_enter();
        kprintf("vasview: update_view: invalid view state\n");
        panic("unexpected behavior");
    }
}

static void update_info(void) {
    term_clear_rows(INFO_START_ROW, INFO_ROWS);
    term_put_cursor_at(INFO_START_ROW, 0);

    uint32_t entry = entry_at_cursor();
    uint32_t start_addr = (g_dir_idx << 22);
    uint32_t end_addr = (start_addr + 0x400000);

    if (VIEW_TBL == g_view) {
        start_addr += (g_tbl_idx << 12);
        end_addr = (start_addr + 4096);
    }

    if (VIEW_DIR == g_view) {
        kprintf("  Dir index: %4u\n", idx_at_cursor());
    } else if (VIEW_TBL == g_view) {
        kprintf("  Dir index: %4u     Table index: %4u\n", g_dir_idx,
                g_tbl_idx);
    }

    kprintf("  Address range: %08x .. %s%08x\n", start_addr,
            ((0 == end_addr) ? "1" : ""), // HACK: prepend 1 to 00000000
            end_addr);

    kprintf("   ADDRESS  FLAGS     DPL  R/W  PRESENT\n");
    kprintf("  %08x    %03x  %s  %s  %s", (entry & ~0xFFF), (entry & 0xFFF),
            ((entry & FLAG_ANY_DPL) ? "   any" : "kernel"),
            ((entry & FLAG_WRITABLE) ? "yes" : " no"),
            ((entry & FLAG_PRESENT) ? "    yes" : "     no"));
}

static void update_cursor(void) {
    // Convert current level index to row and column.
    uint32_t idx = idx_at_cursor();
    uint32_t row = (VIEW_START_ROW + (idx / VIEW_COLS));
    uint32_t col = (VIEW_START_COL + (idx % VIEW_COLS));

    term_put_cursor_at(row, col);
}

static void show_dir(void) {
    term_clear();
    kprintf("Dir %P", gp_pgdir);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++) {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++) {
            uint32_t dir_idx = ((view_row * VIEW_COLS) + view_col);
            if (dir_idx >= 1024) { continue; }

            uint32_t entry = gp_pgdir[dir_idx];
            if (entry & FLAG_PRESENT) {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col), '+');
            } else {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col), '.');
            }
        }
    }

    update_cursor();
}

static void show_tbl(void) {
    uint32_t *p_tbl = ((uint32_t *)(gp_pgdir[g_dir_idx] & ~0xFFF));

    term_clear();
    kprintf("Dir %P, table %P", gp_pgdir, p_tbl);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++) {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++) {
            uint32_t tbl_idx = ((view_row * VIEW_COLS) + view_col);
            if (tbl_idx >= 1024) { continue; }

            uint32_t entry = p_tbl[tbl_idx];
            if (entry & FLAG_PRESENT) {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col), '+');
            } else {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col), '.');
            }
        }
    }

    update_cursor();
}

static uint32_t entry_at_cursor(void) {
    uint32_t entry;
    switch (g_view) {
    case VIEW_DIR:
        entry = gp_pgdir[g_dir_idx];
        break;
    case VIEW_TBL: {
        uint32_t *p_tbl = ((uint32_t *)(gp_pgdir[g_dir_idx] & ~0xFFF));
        entry = p_tbl[g_tbl_idx];
    } break;

    default:
        panic_enter();
        kprintf("vasview: entry_at_cursor: invalid view state\n");
        panic("unexpected behavior");
    }
    return entry;
}

static uint32_t idx_at_cursor(void) {
    uint32_t idx;
    switch (g_view) {
    case VIEW_DIR:
        idx = g_dir_idx;
        break;
    case VIEW_TBL:
        idx = g_tbl_idx;
        break;

    default:
        panic_enter();
        kprintf("vasview: idx_at_cursor: invalid view state\n");
        panic("unexpected behavior");
    }
    return idx;
}

static void parse_kbd_event(kbd_event_t *p_event) {
    if (p_event->b_released) { return; }

    term_acquire_mutex();

    switch (p_event->key) {
    case KEY_LEFTARROW:
    case KEY_H:
        move_cursor(-1);
        break;
    case KEY_RIGHTARROW:
    case KEY_L:
        move_cursor(+1);
        break;
    case KEY_UPARROW:
    case KEY_K:
        move_cursor(-VIEW_COLS);
        break;
    case KEY_DOWNARROW:
    case KEY_J:
        move_cursor(+VIEW_COLS);
        break;

    case KEY_SPACE:
        update_full();
        break;

    case KEY_ENTER:
        deeper_view();
        break;
    case KEY_ESCAPE:
        shallower_view();
        break;
    }

    term_release_mutex();
}

static void move_cursor(int32_t inc_idx) {
    volatile uint32_t *p_idx;
    switch (g_view) {
    case VIEW_DIR:
        p_idx = &g_dir_idx;
        break;
    case VIEW_TBL:
        p_idx = &g_tbl_idx;
        break;

    default:
        panic_enter();
        kprintf("vasview: move_cursor: invalid view state\n");
        panic("unexpected behvaior");
    }

    int32_t new_idx = (((int32_t)(*p_idx)) + inc_idx);
    if ((0 <= new_idx) && (new_idx < 1024)) {
        *p_idx += inc_idx;
        update_info();
        update_cursor();
    }
}

static void deeper_view(void) {
    switch (g_view) {
    case VIEW_DIR:
        if (entry_at_cursor() & FLAG_PRESENT) { g_view = VIEW_TBL; }
        break;

    case VIEW_TBL:
        // Do nothing.
        break;

    default:
        panic_enter();
        kprintf("vasview: shallower_view: invalid view state\n");
        panic("unexpected behavior");
    }

    update_full();
}

static void shallower_view(void) {
    switch (g_view) {
    case VIEW_DIR:
        gb_exit = true;
        break;
    case VIEW_TBL:
        g_view = VIEW_DIR;
        break;

    default:
        panic_enter();
        kprintf("vasview: shallower_view: invalid view state\n");
        panic("unexpected behavior");
    }

    update_full();
}
