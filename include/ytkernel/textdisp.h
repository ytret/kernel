#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "kbd.h"

void textdisp_early_init(void);
void textdisp_init(void);
void textdisp_map_iomem(void);
[[gnu::noreturn]] void textdisp_task(void);

/* Output-related functions */
void textdisp_lock(void);
void textdisp_unlock(void);
bool textdisp_owns_lock(void);
void textdisp_begin_panic(void);

void textdisp_clear(void);
void textdisp_clear_rows(size_t start_row, size_t num_rows);

void textdisp_print_str(char const *p_str);
void textdisp_print_str_len(char const *p_str, size_t len);

void textdisp_put_char_at(size_t row, size_t col, char ch);
void textdisp_put_cursor_at(size_t row, size_t col);

size_t textdisp_row(void);
size_t textdisp_col(void);

size_t textdisp_height(void);
size_t textdisp_width(void);

/* Input-related functions */
void textdisp_read_kbd_event(kbd_event_t *p_event);
