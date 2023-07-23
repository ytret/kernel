#include <kbd.h>
#include <kshell/vasview.h>
#include <panic.h>
#include <printf.h>
#include <term.h>

#include <stdbool.h>
#include <stdint.h>

#define VIEW_START_ROW  2
#define VIEW_START_COL  3
#define VIEW_ROWS       16
#define VIEW_COLS       64

#define VIEW_DIR        0
#define VIEW_TBL        1

#define INFO_START_ROW  (VIEW_START_ROW + VIEW_ROWS + 1)
#define INFO_ROWS       4

#define FLAG_PRESENT    (1 << 0)
#define FLAG_WRITABLE   (1 << 1)
#define FLAG_ANY_DPL    (1 << 2)

static bool gb_exit;

static int        g_view;
static uint32_t * gp_pgdir;

// Cursors on level 1 (dir) and level 2 (table).
//
static size_t g_dir_idx;
static size_t g_tbl_idx;

static void update(void);
static void update_view(void);
static void update_info(void);
static void update_cursor(void);

static void show_dir(void);
static void show_tbl(void);

static uint32_t entry_at_cursor(void);
static uint32_t idx_at_cursor(void);

static void kbd_callback(uint8_t key, bool b_released);
static void move_cursor(int32_t inc_idx);
static void deeper_view(void);
static void shallower_view(void);

void
vasview (uint32_t pgdir_virt)
{
    kbd_set_callback(kbd_callback);

    gb_exit   = false;
    gp_pgdir  = ((uint32_t *) pgdir_virt);
    g_dir_idx = 0;
    g_tbl_idx = 0;

    update();

    while (!gb_exit)
    {}

    // Before returning to kshell, put the cursor on the last row, to make the
    // prompt appear there.
    //
    term_put_cursor_at((term_height() - 1), 0);
}

static void
update (void)
{
    update_view();
    update_info();
    update_cursor();
}

static void
update_view (void)
{
    switch (g_view)
    {
        case VIEW_DIR: show_dir(); break;
        case VIEW_TBL: show_tbl(); break;

        default:
            printf("vasview: update_view: invalid view state\n");
            panic("unexpected behavior");
    }
}

static void
update_info (void)
{
    term_clear_rows(INFO_START_ROW, INFO_ROWS);
    term_put_cursor_at(INFO_START_ROW, 0);

    uint32_t entry      = entry_at_cursor();
    uint32_t start_addr = (g_dir_idx << 22);
    uint32_t end_addr   = (start_addr + 0x400000);

    if (VIEW_TBL == g_view)
    {
        start_addr += (g_tbl_idx << 12);
        end_addr   = (start_addr + 4096);
    }

    if (VIEW_DIR == g_view)
    {
        printf("  Dir index: %4u\n", idx_at_cursor());
    }
    else if (VIEW_TBL == g_view)
    {
        printf("  Dir index: %4u     Table index: %4u\n", g_dir_idx, g_tbl_idx);
    }

    printf("  Address range: %08x .. %s%08x\n",
           start_addr,
           ((0 == end_addr) ? "1" : ""),  // HACK: prepend 1 to 00000000
           end_addr);

    printf("   ADDRESS  FLAGS     DPL  R/W  PRESENT\n");
    printf("  %08x    %03x  %s  %s  %s",
           (entry & ~0xFFF),
           (entry & 0xFFF),
           ((entry & FLAG_ANY_DPL) ? "   any" : "kernel"),
           ((entry & FLAG_WRITABLE) ? "yes" : " no"),
           ((entry & FLAG_PRESENT) ? "    yes": "     no"));
}

static void
update_cursor (void)
{
    // Convert current level index to row and column.
    //
    uint32_t idx = idx_at_cursor();
    uint32_t row = (VIEW_START_ROW + (idx / VIEW_COLS));
    uint32_t col = (VIEW_START_COL + (idx % VIEW_COLS));

    term_put_cursor_at(row, col);
}

static void
show_dir (void)
{
    term_clear();
    printf("Dir %P", gp_pgdir);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++)
    {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++)
        {
            uint32_t dir_idx = ((view_row * VIEW_COLS) + view_col);
            if (dir_idx >= 1024)
            {
                continue;
            }

            uint32_t entry = gp_pgdir[dir_idx];
            if (entry & FLAG_PRESENT)
            {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col),
                                 '+');
            }
            else
            {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col),
                                 '.');
            }
        }
    }

    update_cursor();
}

static void
show_tbl (void)
{
    uint32_t * p_tbl = ((uint32_t *) (gp_pgdir[g_dir_idx] & ~0xFFF));

    term_clear();
    printf("Dir %P, table %P", gp_pgdir, p_tbl);

    for (size_t view_row = 0; view_row < VIEW_ROWS; view_row++)
    {
        for (size_t view_col = 0; view_col < VIEW_COLS; view_col++)
        {
            uint32_t tbl_idx = ((view_row * VIEW_COLS) + view_col);
            if (tbl_idx >= 1024)
            {
                continue;
            }

            uint32_t entry = p_tbl[tbl_idx];
            if (entry & FLAG_PRESENT)
            {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col),
                                 '+');
            }
            else
            {
                term_put_char_at((VIEW_START_ROW + view_row),
                                 (VIEW_START_COL + view_col),
                                 '.');
            }
        }
    }

    update_cursor();
}

static uint32_t
entry_at_cursor (void)
{
    uint32_t entry;
    switch (g_view)
    {
        case VIEW_DIR: entry = gp_pgdir[g_dir_idx]; break;
        case VIEW_TBL:
        {
            uint32_t * p_tbl = ((uint32_t *) (gp_pgdir[g_dir_idx] & ~0xFFF));
            entry = p_tbl[g_tbl_idx];
        }
        break;

        default:
            printf("vasview: entry_at_cursor: invalid view state\n");
            panic("unexpected behavior");
    }
    return (entry);
}

static uint32_t
idx_at_cursor (void)
{
    uint32_t idx;
    switch (g_view)
    {
        case VIEW_DIR: idx = g_dir_idx; break;
        case VIEW_TBL: idx = g_tbl_idx; break;

        default:
            printf("vasview: idx_at_cursor: invalid view state\n");
            panic("unexpected behavior");
    }
    return (idx);
}

static void
kbd_callback (uint8_t key, bool b_released)
{
    if (b_released)
    {
        return;
    }

    switch (key)
    {
        case KEY_LEFTARROW:  move_cursor(-1); break;
        case KEY_RIGHTARROW: move_cursor(+1); break;
        case KEY_UPARROW:    move_cursor(-VIEW_COLS); break;
        case KEY_DOWNARROW:  move_cursor(+VIEW_COLS); break;

        case KEY_SPACE:      update(); break;

        case KEY_ENTER:      deeper_view();    break;
        case KEY_ESCAPE:     shallower_view(); break;

        default:
            return;
    }
}

static void
move_cursor (int32_t inc_idx)
{
    uint32_t * p_idx;
    switch (g_view)
    {
        case VIEW_DIR: p_idx = &g_dir_idx; break;
        case VIEW_TBL: p_idx = &g_tbl_idx; break;

        default:
            printf("vasview: move_cursor: invalid view state\n");
            panic("unexpected behvaior");
    }

    int32_t new_idx = (((int32_t) (*p_idx)) + inc_idx);
    if ((0 <= new_idx) && (new_idx < 1024))
    {
        *p_idx += inc_idx;
        update();
    }
}

static void
deeper_view (void)
{
    switch (g_view)
    {
        case VIEW_DIR:
            if (entry_at_cursor() & FLAG_PRESENT)
            {
                g_view    = VIEW_TBL;
            }
        break;

        case VIEW_TBL:
            // Do nothing.
            //
        break;

        default:
            printf("vasview: shallower_view: invalid view state\n");
            panic("unexpected behavior");
    }

    update();
}

static void
shallower_view (void)
{
    switch (g_view)
    {
        case VIEW_DIR: gb_exit = true;    break;
        case VIEW_TBL: g_view = VIEW_DIR; break;

        default:
            printf("vasview: shallower_view: invalid view state\n");
            panic("unexpected behavior");
    }

    update();
}
