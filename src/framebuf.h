/**
 * @file framebuf.h
 * Framebuffer terminal.
 *
 * See #output_impl_t in @link term.c @endlink for the terminal interface that
 * this module implements.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void framebuf_init(void);
void framebuf_map_iomem(void);

uint32_t framebuf_start(void);
uint32_t framebuf_end(void);

size_t framebuf_height_chars(void);
size_t framebuf_width_chars(void);

/**
 * Places a character on the last shadow screen.
 */
void framebuf_put_char_at(size_t lss_row, size_t lss_col, char ch);

/**
 * Moves the cursor to a position on the last shadow screen.
 *
 * Places a new cursor on the last shadow screen, deleting the previous one.
 *
 * @note
 * If the cursor is not enabled, this function only deletes it and does not draw
 * the new one at (lss_row; lss_col).
 */
void framebuf_put_cursor_at(size_t lss_row, size_t lss_col);

/**
 * Clears rows on the last shadow screen.
 */
void framebuf_clear_rows(size_t lss_start_row, size_t lss_num_rows);

/**
 * Scrolls the shadow buffer so that one empty row is available at the bottom.
 */
void framebuf_scroll_new_row(void);

void framebuf_init_history(void);

/**
 * Clears the shadow buffer, deleting all output history.
 *
 * @warning
 * This function also clears the corresponding rows of the currently visible
 * part, **without** updating the framebuffer.
 */
void framebuf_clear_history(void);

size_t framebuf_history_screens(void);

/**
 * Returns the row number between the first visible row and the history start.
 *
 * In other words, returns the index of the shadow buffer row corresponding to
 * the first visible row.
 */
size_t framebuf_history_pos(void);

void framebuf_set_history_mode(size_t row_from_start);
bool framebuf_is_history_mode_active(void);
