#pragma once

#include "textdisp.h"

typedef struct console console_t;

console_t *console_get_boot_con(void);

void console_init(console_t *con);
void console_lock(console_t *con);
void console_unlock(console_t *con);

bool console_attach(console_t *con, textdisp_t *disp);
textdisp_t *console_detach(console_t *con);

bool console_is_ready(console_t *con);
size_t console_cursor_row(console_t *con);
size_t console_cursor_col(console_t *con);
size_t console_rows(console_t *con);
size_t console_cols(console_t *con);

void console_clear(console_t *con);
void console_clear_rows(console_t *con, size_t start_row, size_t num_rows);

void console_put_cursor_at(console_t *con, size_t row, size_t col);
void console_put_char_at(console_t *con, size_t row, size_t col, char ch);
void console_put_char(console_t *con, char ch);
void console_put_str(console_t *con, const char *str);
