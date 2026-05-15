#pragma once

#include "textdisp.h"

typedef struct {
    textdisp_t *disp;

    size_t rows;
    size_t cols;

    char *cache;
    size_t cache_size;
    size_t cache_row_pitch;

    size_t cursor_row;
    size_t cursor_col;
} console_t;

console_t *console_get_boot_con(void);

void console_init(console_t *con);

bool console_attach(console_t *con, textdisp_t *disp);
bool console_detach(console_t *con);

void console_clear(console_t *con);
void console_clear_rows(console_t *con, size_t start_row, size_t num_rows);

void console_put_cursor_at(console_t *con, size_t row, size_t col);
void console_put_char_at(console_t *con, size_t row, size_t col, char ch);
void console_put_char(console_t *con, char ch);
void console_put_str(console_t *con, const char *str);
