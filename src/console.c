#include "assert.h"
#include "console.h"
#include "heap.h"
#include "log.h"
#include "memfun.h"
#include "pmm.h"

#define CONSOLE_CACHE_CHAR_SIZE 1
#define CONSOLE_CACHE_ALIGN     PMM_PAGE_SIZE

static console_t g_console_boot_con;

static void prv_console_scroll(console_t *con);

static void prv_console_redraw_cache(console_t *con);
static void prv_console_realloc_cache(console_t *con);
static void prv_console_clear_cache(console_t *con, size_t start_row,
                                    size_t num_rows);

[[gnu::artificial]]
static inline void assert_owns_mutex(console_t *con) {
    if (!mutex_caller_owns(&con->lock)) { panic_nested(); }
}

console_t *console_get_boot_con(void) {
    return &g_console_boot_con;
}

void console_init(console_t *con) {
    kmemset(con, 0, sizeof(*con));
    mutex_init(&con->lock);
    con->ready = true;
}

void console_lock(console_t *con) {
    if (!mutex_caller_owns(&con->lock)) { mutex_acquire(&con->lock); }
    con->lock_cnt++;
    textdisp_lock(con->disp);
}

void console_unlock(console_t *con) {
    ASSERT(con->lock_cnt > 0);
    con->lock_cnt--;
    textdisp_unlock(con->disp);
    if (con->lock_cnt == 0) { mutex_release(&con->lock); }
}

bool console_attach(console_t *con, textdisp_t *disp) {
    DEBUG_ASSERT(con != NULL);
    DEBUG_ASSERT(disp != NULL);

    assert_owns_mutex(con);

    if (con->disp) {
        LOG_ERROR("console %p is already attached to textdisp %p", con,
                  con->disp);
        return false;
    }

    con->rows = textdisp_height(disp);
    con->cols = textdisp_width(disp);
    con->disp = disp;

    ASSERT(con->rows != 0);
    ASSERT(con->cols != 0);

    textdisp_lock(con->disp);
    prv_console_redraw_cache(con);
    textdisp_unlock(con->disp);

    return true;
}

bool console_detach(console_t *con) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    PANIC("TODO %s", __func__);
}

void console_clear(console_t *con) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    prv_console_clear_cache(con, 0, con->rows);

    textdisp_lock(con->disp);
    textdisp_clear(con->disp);
    textdisp_unlock(con->disp);

    console_put_cursor_at(con, 0, 0);
}

void console_clear_rows(console_t *con, size_t start_row, size_t num_rows) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    prv_console_clear_cache(con, start_row, num_rows);

    textdisp_lock(con->disp);
    textdisp_clear_rows(con->disp, start_row, num_rows);
    textdisp_unlock(con->disp);
}

void console_put_cursor_at(console_t *con, size_t row, size_t col) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    DEBUG_ASSERT(row < con->rows);
    DEBUG_ASSERT(col < con->cols);

    if (con->cursor_row == row && con->cursor_col == col) { return; }

    con->cursor_row = row;
    con->cursor_col = col;

    textdisp_lock(con->disp);
    textdisp_put_cursor_at(con->disp, row, col);
    textdisp_unlock(con->disp);
}

void console_put_char_at(console_t *con, size_t row, size_t col, char ch) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    DEBUG_ASSERT(row < con->rows);
    DEBUG_ASSERT(col < con->cols);

    con->cache[col + row * con->cache_row_pitch] = ch;

    textdisp_lock(con->disp);
    textdisp_put_char_at(con->disp, row, col, ch);
    textdisp_unlock(con->disp);
}

void console_put_char(console_t *con, char ch) {
    DEBUG_ASSERT(con != NULL);

    assert_owns_mutex(con);

    size_t row = con->cursor_row;
    size_t col = con->cursor_col;

    switch (ch) {
    case '\n':
        col = 0;
        row++;
        if (row >= con->rows) {
            row = con->rows - 1;
            prv_console_scroll(con);
        }
        break;

    default:
        console_put_char_at(con, row, col, ch);

        col++;
        if (col >= con->cols) {
            col = 0;
            row++;
            if (row >= con->rows) {
                row = (con->rows - 1);
                prv_console_scroll(con);
            }
        }
    }

    console_put_cursor_at(con, row, col);
}

void console_put_str(console_t *con, const char *str) {
    DEBUG_ASSERT(con != NULL);
    DEBUG_ASSERT(str != NULL);

    assert_owns_mutex(con);

    char ch;
    while ((ch = *str++)) {
        console_put_char(con, ch);
    }
}

static void prv_console_scroll(console_t *con) {
    kmemmove(con->cache, &con->cache[con->cache_row_pitch],
             con->cache_row_pitch * (con->rows - 1));

    textdisp_scroll(con->disp);
}

static void prv_console_redraw_cache(console_t *con) {
    prv_console_realloc_cache(con);

    for (size_t row = 0; row < con->rows; row++) {
        for (size_t col = 0; col < con->cols; col++) {
            const char ch = con->cache[col + row * con->cache_row_pitch];
            textdisp_put_char_at(con->disp, row, col, ch);
        }
    }
}

static void prv_console_realloc_cache(console_t *con) {
    ASSERT(con->rows != 0);
    ASSERT(con->cols != 0);

    con->cache_row_pitch = con->cols * CONSOLE_CACHE_CHAR_SIZE;
    con->cache_size = con->rows * con->cache_row_pitch;
    con->cache = heap_realloc(con->cache, con->cache_size, CONSOLE_CACHE_ALIGN);

    prv_console_clear_cache(con, 0, con->rows);
}

static void prv_console_clear_cache(console_t *con, size_t start_row,
                                    size_t num_rows) {
    ASSERT(start_row < con->rows);
    ASSERT(num_rows <= con->rows);
    ASSERT(start_row + num_rows <= con->rows);

    kmemset(&con->cache[start_row * con->cache_row_pitch], ' ',
            num_rows * con->cache_row_pitch);
}
