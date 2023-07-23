#include <kbd.h>
#include <kshell/vasview.h>
#include <printf.h>
#include <term.h>

#include <stdbool.h>
#include <stdint.h>

#define VIEW_START_ROW  2
#define VIEW_START_COL  3
#define VIEW_ROWS       16
#define VIEW_COLS       64

#define INFO_START_ROW  (VIEW_START_ROW + VIEW_ROWS + 1)
#define INFO_ROWS       4

#define FLAG_PRESENT    (1 << 0)
#define FLAG_WRITABLE   (1 << 1)
#define FLAG_ANY_DPL    (1 << 2)

static bool gb_exit;

static uint32_t * gp_pgdir;

// Selected entry position.
//
static size_t g_row;
static size_t g_col;

static void     update_view(void);
static void     update_cursor(void);
static void     update_info(void);
static uint32_t cursor_to_idx(void);

static void show_pgdir(void);

static void kbd_callback(uint8_t key, bool b_released);
static void move_cursor(int32_t inc_row, int32_t inc_col);
static bool move_check_pos(size_t row, size_t col);

void
vasview (uint32_t pgdir_virt)
{
    kbd_set_callback(kbd_callback);

    gb_exit  = false;
    gp_pgdir = ((uint32_t *) pgdir_virt);
    g_row    = 0;
    g_col    = 0;

    update_view();

    while (!gb_exit)
    {}

    // Before returning to kshell, put the cursor on the last row, to make the
    // prompt appear there.
    //
    term_put_cursor_at((term_height() - 1), 0);
}

static void
update_view (void)
{
    show_pgdir();
}

static void
update_cursor (void)
{
    update_info();
    term_put_cursor_at((VIEW_START_ROW + g_row), (VIEW_START_COL + g_col));
}

static void
update_info (void)
{
    term_clear_rows(INFO_START_ROW, INFO_ROWS);
    term_put_cursor_at(INFO_START_ROW, 0);

    uint32_t dir_idx    = cursor_to_idx();
    uint32_t entry      = gp_pgdir[dir_idx];
    uint32_t start_page = (dir_idx << 22);
    uint32_t end_page   = (start_page + 0x400000);

    printf("  Entry index:   %u\n", cursor_to_idx());
    printf("  Address range: %08x .. %s%08x\n",
           start_page,
           ((0 == end_page) ? "1" : ""),  // HACK: prepend 1 to 00000000
           end_page);

    printf("  FLAGS     DPL  R/W  PRESENT\n");
    printf("    %03x  %s  %s  %s",
           (entry & 0xFFF),
           ((entry & FLAG_ANY_DPL) ? "   any" : "kernel"),
           ((entry & FLAG_WRITABLE) ? "yes" : " no"),
           ((entry & FLAG_PRESENT) ? "    yes": "     no"));
}

static uint32_t
cursor_to_idx (void)
{
    return ((g_row * VIEW_COLS) + g_col);
}

static void
show_pgdir (void)
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
kbd_callback (uint8_t key, bool b_released)
{
    if (b_released)
    {
        return;
    }

    switch (key)
    {
        case KEY_LEFTARROW:  move_cursor(0, -1); break;
        case KEY_RIGHTARROW: move_cursor(0, +1); break;
        case KEY_UPARROW:    move_cursor(-1, 0); break;
        case KEY_DOWNARROW:  move_cursor(+1, 0); break;

        case KEY_SPACE:      update_view(); break;

        case KEY_ESCAPE:
            gb_exit = true;
        break;

        default:
            return;
    }
}

static void
move_cursor (int32_t inc_row, int32_t inc_col)
{
    if (move_check_pos((g_row + inc_row), (g_col + inc_col)))
    {
        g_row += inc_row;
        g_col += inc_col;

        update_cursor();
    }
}

static bool
move_check_pos (size_t row, size_t col)
{
    return ((row < VIEW_ROWS) && (col < VIEW_COLS));
}
