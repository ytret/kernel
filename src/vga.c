/*
 * Text-mode terminal implementation.
 *
 * Uses a sort of double buffering for saving history output and replaying it.
 * The output is first saved in the 'shadow buffer', and, if it's visible,
 * replayed to the VGA framebuffer.
 *
 * Variable naming conventions:
 *   sh_  - shadow buffer
 *   lss_ - last screen of the shadow buffer (LSS - last shadow screen)
 *   vga_ - VGA framebuffer
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert.h"
#include "memfun.h"
#include "port.h"

#define NUM_ROWS 25
#define NUM_COLS 80
#define PITCH    (2 * NUM_COLS)

/*
 * Number of screens of output that the shadow buffer can hold.
 */
#define SHADOW_SCREENS 3

#define PORT_CRTC_ADDR ((uint16_t)0x03D4)
#define PORT_CRTC_DATA ((uint16_t)0x03D5)

/*
 * Hardware defines.
 */
#define VGA_MEMORY_ADDR 0xB8000

#define REG_CRTC_MAX_SCAN_LINE ((uint8_t)0x09)
#define REG_CRTC_CURSOR_START  ((uint8_t)0x0A)
#define REG_CRTC_CURSOR_END    ((uint8_t)0x0B)
#define REG_CRTC_CURSOR_LOC_HI ((uint8_t)0x0E)
#define REG_CRTC_CURSOR_LOC_LO ((uint8_t)0x0F)

#define REG_CRTC_CURSOR_START_CD (1 << 5)

static uint16_t *const gp_vga_buf = (uint16_t *)VGA_MEMORY_ADDR;
static uint16_t gp_shadow_buf[NUM_ROWS * NUM_COLS * SHADOW_SCREENS];
static size_t g_visible_from_row;

static void enable_cursor(void);
static void disable_cursor(void);

static bool get_vga_idx(size_t lss_row, size_t lss_col, size_t *p_vga_idx);
static void get_vga_row_range(size_t lss_start_row, size_t lss_num_rows,
                              size_t *p_vga_start_row, size_t *p_vga_num_rows);
static void copy_shadow_to_vga(void);

void vga_init(void) {
    enable_cursor();
}

size_t vga_height_chars(void) {
    return NUM_ROWS;
}

size_t vga_width_chars(void) {
    return NUM_COLS;
}

/*
 * Places a character on the last screen of the shadow buffer.
 */
void vga_put_char_at(size_t row, size_t col, char ch) {
    ASSERT(row < NUM_ROWS && col < NUM_COLS);

    size_t sh_idx = ((SHADOW_SCREENS - 1) * NUM_ROWS + row) * NUM_COLS + col;
    gp_shadow_buf[sh_idx] = (0x0F << 8) | ch;

    size_t vga_idx;
    bool b_visible = get_vga_idx(row, col, &vga_idx);
    if (b_visible) { gp_vga_buf[vga_idx] = (0x0F << 8) | ch; }
}

/*
 * Positions the cursor on the last screen of the shadow buffer.
 */
void vga_put_cursor_at(size_t row, size_t col) {
    ASSERT(row < NUM_ROWS && col < NUM_COLS);

    size_t char_idx = (row * NUM_COLS + col);
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_HI);
    port_outb(PORT_CRTC_DATA, ((uint8_t)(char_idx >> 8)));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_LO);
    port_outb(PORT_CRTC_DATA, ((uint8_t)char_idx));
}

/*
 * Clears rows in the last screen of the shadow buffer.
 */
void vga_clear_rows(size_t lss_start_row, size_t lss_num_rows) {
    size_t shadow_start_row = (SHADOW_SCREENS - 1) * NUM_ROWS + lss_start_row;
    size_t shadow_num_words = lss_num_rows * NUM_COLS;
    memset_word(&gp_shadow_buf[shadow_start_row * NUM_COLS], 0x0F << 8,
                shadow_num_words);

    size_t vga_start_row;
    size_t vga_num_rows;
    get_vga_row_range(lss_start_row, lss_num_rows, &vga_start_row,
                      &vga_num_rows);
    size_t vga_num_words = vga_num_rows * NUM_COLS;
    memset_word(&gp_vga_buf[vga_start_row * NUM_COLS], 0x0F << 8,
                vga_num_words);
}

/*
 * Scrolls the shadow buffer so that one empty row is available at the bottom.
 */
void vga_scroll_new_row(void) {
    // Move every shadow row except the first one up.
    memmove(gp_shadow_buf, &gp_shadow_buf[1 * NUM_COLS],
            (SHADOW_SCREENS * NUM_ROWS - 1) * PITCH);

    // Clear the last shadow row.
    size_t shadow_last_row = SHADOW_SCREENS * NUM_ROWS - 1;
    memset_word(&gp_shadow_buf[shadow_last_row * NUM_COLS], 0x0F << 8,
                NUM_COLS);

    copy_shadow_to_vga();
}

/*
 * Resets the shadow buffer thus deleting all output history. Also deletes the
 * currently visible part of it.
 */
void vga_clear_history(void) {
    memset_word(gp_shadow_buf, 0x0F << 8, SHADOW_SCREENS * NUM_ROWS * NUM_COLS);
    g_visible_from_row = (SHADOW_SCREENS - 1) * NUM_ROWS;
}

/*
 * Returns the number of screens that the shadow buffer can hold.
 */
size_t vga_history_screens(void) {
    return SHADOW_SCREENS;
}

/*
 * Returns the row number in the shadow buffer of the first visible row.
 */
size_t vga_history_pos(void) {
    return g_visible_from_row;
}

/*
 * Specifies the visible part of the shadow buffer.
 */
void vga_set_history_mode(size_t row_from_start) {
    // Assert that there are enough rows until the shadow buffer end to fill up
    // the visible screen.
    ASSERT(row_from_start <= (SHADOW_SCREENS - 1) * NUM_ROWS);

    g_visible_from_row = row_from_start;
    copy_shadow_to_vga();

    if (g_visible_from_row < (SHADOW_SCREENS - 1) * NUM_ROWS) {
        disable_cursor();
    } else {
        enable_cursor();
    }
}

bool vga_is_history_mode_active(void) {
    return g_visible_from_row < (SHADOW_SCREENS - 1) * NUM_ROWS;
}

static void enable_cursor(void) {
    port_outb(PORT_CRTC_ADDR, REG_CRTC_MAX_SCAN_LINE);
    uint8_t max_scan_line = (port_inb(PORT_CRTC_DATA) & 0x1F);

    uint8_t start = 1;
    uint8_t end = (max_scan_line - 1);

    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_START);
    port_outb(PORT_CRTC_DATA, ((port_inb(PORT_CRTC_DATA) & 0xC0) | start));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_END);
    port_outb(PORT_CRTC_DATA, ((port_inb(PORT_CRTC_DATA) & 0xC0) | end));
}

static void disable_cursor(void) {
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_START);
    port_outb(PORT_CRTC_DATA, REG_CRTC_CURSOR_START_CD);
}

/*
 * Converts the position on the last shadow buffer screen to a VGA framebuffer
 * offset, if the position is visible.
 *
 * Returns true if the position is visible and *p_vga_idx has been written.
 */
static bool get_vga_idx(size_t lss_row, size_t lss_col, size_t *p_vga_idx) {
    /*
     * Explanation by example:
     *
     * The shadow buffer has 3 screens, or 75 rows (NUM_ROWS * 3). The last
     * shadow screen starts at row 50 and ends at row 75 exclusively. Let's say
     * the user visible part begins at row 49 (g_visible_from_row = 49) and ends
     * at +NUM_ROWS = 74 exclusively. Every row of the last shadow screen is
     * visible, except for the last one (rows 74). This check can be expressed
     * as:
     *
     *   bool is_visible = lss_row >= 49 && lss_row < 74
     *
     * So, the visible part of the history buffer is composed of the last row of
     * the second-to-last shadow screen (row 49) and rows [50, 73] of the last
     * shadow screen.
     */

    size_t shadow_row = (SHADOW_SCREENS - 1) * NUM_ROWS + lss_row;
    if (shadow_row >= g_visible_from_row) {
        size_t visible_row = shadow_row - g_visible_from_row;
        *p_vga_idx = visible_row * NUM_COLS + lss_col;
        return true;
    } else {
        return false;
    }
}

/*
 * Similar to get_vga_idx, but converts row ranges on the last shadow screen to
 * their visible counterpart.
 */
static void get_vga_row_range(size_t lss_start_row, size_t lss_num_rows,
                              size_t *p_vga_start_row, size_t *p_vga_num_rows) {
    size_t shadow_start_row = (SHADOW_SCREENS - 1) * NUM_ROWS + lss_start_row;
    size_t shadow_end_row = shadow_start_row + lss_num_rows;

    if (shadow_start_row >= g_visible_from_row &&
        shadow_end_row <= g_visible_from_row + NUM_ROWS) {
        *p_vga_start_row = shadow_start_row - g_visible_from_row;
        *p_vga_num_rows = shadow_end_row - *p_vga_start_row;
    } else {
        *p_vga_start_row = 0;
        *p_vga_num_rows = 0;
    }
}

static void copy_shadow_to_vga(void) {
    memcpy(gp_vga_buf, &gp_shadow_buf[g_visible_from_row * NUM_COLS],
           NUM_ROWS * PITCH);
}
