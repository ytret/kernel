#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "kbd.h"

void term_init(void);
void term_init_history(size_t num_screens);
__attribute__((noreturn)) void term_task(void);

/* Output-related functions */
void term_acquire_mutex(void);
void term_release_mutex(void);
bool term_owns_mutex(void);
void term_enter_panic_mode(void);

void term_clear(void);
void term_clear_rows(size_t start_row, size_t num_rows);

size_t term_history_pos(void);

void term_print_str(char const *p_str);
void term_print_str_len(char const *p_str, size_t len);

void term_put_char_at(size_t row, size_t col, char ch);
void term_put_cursor_at(size_t row, size_t col);

size_t term_row(void);
size_t term_col(void);

size_t term_height(void);
size_t term_width(void);

/* Input-related functions */
void term_read_kbd_event(kbd_event_t *p_event);
