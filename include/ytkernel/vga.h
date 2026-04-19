/**
 * @file vga.h
 * VGA text-mode terminal.
 *
 * See #output_impl_t in @link term.c @endlink for the terminal interface that
 * this module implements.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

void vga_init(void);
void vga_map_iomem(void);

size_t vga_height_chars(void);
size_t vga_width_chars(void);

/**
 * Places a character on the last shadow screen.
 */
void vga_put_char_at(size_t row, size_t col, char ch);

/**
 * Moves the cursor to a position on the last shadow screen.
 */
void vga_put_cursor_at(size_t row, size_t col);

/**
 * Clears rows on the last shadow screen.
 */
void vga_clear_rows(size_t start_row, size_t num_rows);

/**
 * Scrolls the shadow buffer so that one empty row is available at the bottom.
 */
void vga_scroll_new_row(void);

void vga_init_history(void);

/**
 * Clears the shadow buffer, deleting all output history.
 *
 * @warning
 * This function also clears the rows copying/backing the currently visible
 * part, **without** updating the framebuffer.
 */
void vga_clear_history(void);

/**
 * Returns the number of screens that the shadow buffer can hold.
 */
size_t vga_history_screens(void);

/**
 * Returns the row number between the first visible row and the history start.
 *
 * In other words, returns the index of the shadow buffer row corresponding to
 * the first visible row.
 */
size_t vga_history_pos(void);

/**
 * Controls what part of the history is visible on the screen.
 */
void vga_set_history_mode(size_t row_from_start);

bool vga_is_history_mode_active(void);
