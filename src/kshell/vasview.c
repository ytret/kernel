#include <stdbool.h>
#include <stdint.h>

#include "console.h"
#include "kinttypes.h"
#include "kprintf.h"
#include "kshell/vasview.h"
#include "panic.h"
#include "pmm.h"

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

static console_t *g_vasview_con;

static void update_full(void);
static void update_view(void);
static void update_info(void);
static void update_cursor(void);

static void show_dir(void);
static void show_tbl(void);

static uint32_t entry_at_cursor(void);
static uint32_t idx_at_cursor(void);

static void move_cursor(int32_t inc_idx);
static void deeper_view(void);
static void shallower_view(void);

void vasview(uint32_t pgdir_virt) {
    gb_exit = false;
    gp_pgdir = ((uint32_t *)pgdir_virt);
    g_dir_idx = 0;
    g_tbl_idx = 0;

    g_vasview_con = console_get_boot_con();

    console_lock(g_vasview_con);
    update_full();
    console_unlock(g_vasview_con);

    while (!gb_exit) {
        // TODO
        gb_exit = true;
    }

    // Before returning to kshell, put the cursor on the last row, to make the
    // prompt appear there.
    console_lock(g_vasview_con);
    console_put_cursor_at(g_vasview_con, console_rows(g_vasview_con) - 1, 0);
    console_unlock(g_vasview_con);
}

static void update_full(void) {
    update_view();
    update_info();
    update_cursor();
}

static void update_view(void) {
    switch (g_view) {
    case VIEW_DIR: show_dir(); break;
    case VIEW_TBL: show_tbl(); break;

    default:       PANIC("invalid view state");
    }
}

static void update_info(void) {
    console_clear_rows(g_vasview_con, INFO_START_ROW, INFO_ROWS);
    console_put_cursor_at(g_vasview_con, INFO_START_ROW, 0);

    uint32_t entry = entry_at_cursor();
    uint32_t start_addr = (g_dir_idx << 22);
    uint32_t end_addr = (start_addr + 0x400000);

    if (VIEW_TBL == g_view) {
        start_addr += (g_tbl_idx << 12);
        end_addr = (start_addr + 4096);
    }

    if (VIEW_DIR == g_view) {
        kprintf("  Dir index: %4" PRIu32 "\n", idx_at_cursor());
    } else if (VIEW_TBL == g_view) {
        kprintf("  Dir index: %4" PRIu32 "     Table index: %4" PRIu32 "\n",
                g_dir_idx, g_tbl_idx);
    }

    kprintf("  Address range: %08" PRIx32 " .. %s%08" PRIx32 "\n", start_addr,
            ((0 == end_addr) ? "1" : ""), // HACK: prepend 1 to 00000000
            end_addr);

    if (VIEW_DIR == g_view) {
        kprintf("   ADDRESS  FLAGS     DPL  R/W  PRESENT\n");
        kprintf("  %08" PRIx32 "    %03" PRIx32 "  %s  %s  %s",
                (entry & ~0xFFF), (entry & 0xFFF),
                ((entry & FLAG_ANY_DPL) ? "   any" : "kernel"),
                ((entry & FLAG_WRITABLE) ? "yes" : " no"),
                ((entry & FLAG_PRESENT) ? "    yes" : "     no"));
    } else {
        // FIXME: only works if identity mapped.
        const char *alloc_type_name = "unkwn";
        pmm_page_t *const metadata = pmm_paddr_to_page(start_addr);
        switch (metadata->type) {
        case PMM_ALLOC_NONE:  alloc_type_name = " none"; break;
        case PMM_ALLOC_SLAB:  alloc_type_name = " slab"; break;
        case PMM_ALLOC_LARGE: alloc_type_name = "large"; break;
        }

        const char *region_type_name = "unkwn";
        pmm_region_t *const region = pmm_find_region_by_addr(start_addr);
        switch (region->type) {
        case PMM_REGION_AVAILABLE: region_type_name = "  available RAM"; break;
        case PMM_REGION_RESERVED:  region_type_name = "   reserved RAM"; break;
        case PMM_REGION_KERNEL_RESERVED:
            region_type_name = "kernel reserved";
            break;
        case PMM_REGION_KERNEL_AND_MODS:
            region_type_name = "kernel and mods";
            break;
        case PMM_REGION_STATIC_HEAP:
            region_type_name = "    static heap";
            break;
        }

        kprintf("   ADDRESS  FLAGS     DPL  R/W  PRESENT  ALLOC           "
                "REGION\n");
        kprintf("  %08" PRIx32 "    %03" PRIx32 "  %s  %s  %s  %s  %s",
                (entry & ~0xFFF), (entry & 0xFFF),
                ((entry & FLAG_ANY_DPL) ? "   any" : "kernel"),
                ((entry & FLAG_WRITABLE) ? "yes" : " no"),
                ((entry & FLAG_PRESENT) ? "    yes" : "     no"),
                alloc_type_name, region_type_name);
    }
}

static void update_cursor(void) {
    // Convert current level index to row and column.
    uint32_t idx = idx_at_cursor();
    uint32_t row = (VIEW_START_ROW + (idx / VIEW_COLS));
    uint32_t col = (VIEW_START_COL + (idx % VIEW_COLS));

    console_put_cursor_at(g_vasview_con, row, col);
}

static void show_dir(void) {
    console_clear(g_vasview_con);
    kprintf("Dir %p", gp_pgdir);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++) {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++) {
            uint32_t dir_idx = ((view_row * VIEW_COLS) + view_col);
            if (dir_idx >= 1024) { continue; }

            uint32_t entry = gp_pgdir[dir_idx];
            if (entry & FLAG_PRESENT) {
                console_put_char_at(g_vasview_con, VIEW_START_ROW + view_row,
                                    VIEW_START_COL + view_col, '+');
            } else {
                console_put_char_at(g_vasview_con, VIEW_START_ROW + view_row,
                                    VIEW_START_COL + view_col, '.');
            }
        }
    }

    update_cursor();
}

static void show_tbl(void) {
    uint32_t *p_tbl = ((uint32_t *)(gp_pgdir[g_dir_idx] & ~0xFFF));

    console_clear(g_vasview_con);
    kprintf("Dir %p, table %p", gp_pgdir, p_tbl);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++) {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++) {
            uint32_t tbl_idx = ((view_row * VIEW_COLS) + view_col);
            if (tbl_idx >= 1024) { continue; }

            uint32_t entry = p_tbl[tbl_idx];
            if (entry & FLAG_PRESENT) {
                console_put_char_at(g_vasview_con, VIEW_START_ROW + view_row,
                                    VIEW_START_COL + view_col, '+');
            } else {
                console_put_char_at(g_vasview_con, VIEW_START_ROW + view_row,
                                    VIEW_START_COL + view_col, '.');
            }
        }
    }

    update_cursor();
}

static uint32_t entry_at_cursor(void) {
    uint32_t entry;
    switch (g_view) {
    case VIEW_DIR: entry = gp_pgdir[g_dir_idx]; break;
    case VIEW_TBL: {
        uint32_t *p_tbl = ((uint32_t *)(gp_pgdir[g_dir_idx] & ~0xFFF));
        entry = p_tbl[g_tbl_idx];
    } break;

    default: PANIC("invalid view state");
    }
    return entry;
}

static uint32_t idx_at_cursor(void) {
    uint32_t idx;
    switch (g_view) {
    case VIEW_DIR: idx = g_dir_idx; break;
    case VIEW_TBL: idx = g_tbl_idx; break;

    default:       PANIC("invalid view state");
    }
    return idx;
}

static void move_cursor(int32_t inc_idx) {
    volatile uint32_t *p_idx;
    switch (g_view) {
    case VIEW_DIR: p_idx = &g_dir_idx; break;
    case VIEW_TBL: p_idx = &g_tbl_idx; break;

    default:       PANIC("invalid view state");
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

    default: PANIC("invalid view state");
    }

    update_full();
}

static void shallower_view(void) {
    switch (g_view) {
    case VIEW_DIR: gb_exit = true; break;
    case VIEW_TBL: g_view = VIEW_DIR; break;

    default:       PANIC("invalid view state");
    }

    update_full();
}
