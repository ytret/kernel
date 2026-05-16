#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "assert.h"
#include "console.h"
#include "heap.h"
#include "kmutex.h"
#include "log.h"
#include "memfun.h"
#include "pmm.h"

#define CONSOLE_CACHE_CHAR_SIZE 1
#define CONSOLE_CACHE_ALIGN     PMM_PAGE_SIZE

struct console {
    bool ready;
    task_mutex_t lock;
    size_t lock_cnt;

    textdisp_t *disp;

    size_t rows;
    size_t cols;

    char *cache;
    size_t cache_size;
    size_t cache_row_pitch;

    size_t cursor_row;
    size_t cursor_col;
};

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

console_t *console_new(void) {
    console_t *const con =
        heap_alloc_aligned(sizeof(console_t), _Alignof(console_t));
    console_init(con);
    return con;
}

void console_lock(console_t *con) {
    if (!mutex_caller_owns(&con->lock)) { mutex_acquire(&con->lock); }
    con->lock_cnt++;
}

void console_unlock(console_t *con) {
    ASSERT(con->lock_cnt > 0);
    con->lock_cnt--;
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

    prv_console_redraw_cache(con);

    return true;
}

textdisp_t *console_detach(console_t *con) {
    DEBUG_ASSERT(con != NULL);
    assert_owns_mutex(con);

    textdisp_t *const disp = con->disp;
    con->disp = NULL;
    return disp;
}

bool console_is_ready(console_t *con) {
    return con->ready;
}

size_t console_cursor_row(console_t *con) {
    return con->cursor_row;
}

size_t console_cursor_col(console_t *con) {
    return con->cursor_col;
}

size_t console_rows(console_t *con) {
    return con->rows;
}

size_t console_cols(console_t *con) {
    return con->cols;
}

void console_clear(console_t *con) {
    DEBUG_ASSERT(con != NULL);
    assert_owns_mutex(con);

    prv_console_clear_cache(con, 0, con->rows);
    if (con->disp) { textdisp_clear(con->disp); }
    console_put_cursor_at(con, 0, 0);
}

void console_clear_rows(console_t *con, size_t start_row, size_t num_rows) {
    DEBUG_ASSERT(con != NULL);
    assert_owns_mutex(con);

    prv_console_clear_cache(con, start_row, num_rows);
    if (con->disp) { textdisp_clear_rows(con->disp, start_row, num_rows); }
}

void console_put_cursor_at(console_t *con, size_t row, size_t col) {
    DEBUG_ASSERT(con != NULL);
    assert_owns_mutex(con);
    DEBUG_ASSERT(row < con->rows);
    DEBUG_ASSERT(col < con->cols);

    if (con->cursor_row == row && con->cursor_col == col) { return; }
    con->cursor_row = row;
    con->cursor_col = col;
    if (con->disp) { textdisp_put_cursor_at(con->disp, row, col); }
}

void console_put_char_at(console_t *con, size_t row, size_t col, char ch) {
    DEBUG_ASSERT(con != NULL);
    assert_owns_mutex(con);
    DEBUG_ASSERT(row < con->rows);
    DEBUG_ASSERT(col < con->cols);

    con->cache[col + row * con->cache_row_pitch] = ch;
    if (con->disp) { textdisp_put_char_at(con->disp, row, col, ch); }
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
    kmemset(&con->cache[con->cache_row_pitch * (con->rows - 1)], ' ',
            con->cache_row_pitch);

    if (con->disp) { textdisp_scroll(con->disp); }
}

static void prv_console_redraw_cache(console_t *con) {
    LOG_FLOW("con %p", con);
    ASSERT(con->disp != NULL);

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

    if (con->cache) {
        LOG_FLOW("realloc cache");
        con->cache =
            heap_realloc(con->cache, con->cache_size, CONSOLE_CACHE_ALIGN);
    } else {
        LOG_FLOW("alloc and clear cache");
        con->cache = heap_alloc_aligned(con->cache_size, CONSOLE_CACHE_ALIGN);
        prv_console_clear_cache(con, 0, con->rows);
    }
}

static void prv_console_clear_cache(console_t *con, size_t start_row,
                                    size_t num_rows) {
    ASSERT(start_row < con->rows);
    ASSERT(num_rows <= con->rows);
    ASSERT(start_row + num_rows <= con->rows);

    kmemset(&con->cache[start_row * con->cache_row_pitch], ' ',
            num_rows * con->cache_row_pitch);
}
