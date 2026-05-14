#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "kbd.h"

typedef struct {
    void (*p_init)(void);
    void (*p_map_iomem)(void);

    void (*p_put_char_at)(size_t row, size_t col, char ch);
    void (*p_put_cursor_at)(size_t row, size_t col);
    void (*p_clear_rows)(size_t start_row, size_t num_rows);
    void (*p_scroll_new_row)(void);
} textdisp_ops_t;

typedef struct textdisp textdisp_t;

textdisp_t *textdisp_get_boot_disp(void);

void textdisp_early_init(textdisp_t *disp);
void textdisp_init(textdisp_t *disp);
void textdisp_map_iomem(textdisp_t *disp);

void textdisp_lock(textdisp_t *disp);
void textdisp_unlock(textdisp_t *disp);
bool textdisp_owns_lock(textdisp_t *disp);
void textdisp_begin_panic(textdisp_t *disp);

void textdisp_clear(textdisp_t *disp);
void textdisp_clear_rows(textdisp_t *disp, size_t start_row, size_t num_rows);

void textdisp_print_str(textdisp_t *disp, char const *p_str);
void textdisp_print_str_len(textdisp_t *disp, char const *p_str, size_t len);

void textdisp_put_char_at(textdisp_t *disp, size_t row, size_t col, char ch);
void textdisp_put_cursor_at(textdisp_t *disp, size_t row, size_t col);

size_t textdisp_row(textdisp_t *disp);
size_t textdisp_col(textdisp_t *disp);

size_t textdisp_height(textdisp_t *disp);
size_t textdisp_width(textdisp_t *disp);

void textdisp_read_kbd_event(kbd_event_t *p_event);
