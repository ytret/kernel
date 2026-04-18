/**
 * @file vga.c
 * VGA text-mode terminal implementation.
 *
 * It uses a kind of double buffering to save the output history and replay it.
 * First, the output is saved in a "shadow buffer". If it's visible, it's then
 * copied to the VGA framebuffer.
 *
 * Variable naming convention:
 *
 * - `sh_` -- shadow buffer
 * - `lss_` -- last screen of the shadow buffer (LSS -- last shadow screen)
 * - `vga_` -- VGA framebuffer
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert.h"
#include "memfun.h"
#include "port.h"
#include "vga.h"
#include "vmm.h"

#define VGA_NUM_ROWS 25
#define VGA_NUM_COLS 80
#define VGA_PITCH    (2 * VGA_NUM_COLS)

/**
 * Size of output that the shadow buffer can save in screens.
 */
#define VGA_NUM_SHADOW_SCREENS 3

/**
 * @{
 * @name Memory-mapped register defines
 */
#define VGA_MMIO_START 0xB8000
#define VGA_MMIO_SIZE  (VGA_NUM_ROWS * VGA_PITCH)
#define VGA_MMIO_END   (VGA_MMIO_START + VGA_MMIO_SIZE)

#define VGA_PORT_CRTC_ADDR ((uint16_t)0x03D4)
#define VGA_PORT_CRTC_DATA ((uint16_t)0x03D5)

#define VGA_CRTC_MAX_SCAN_LINE   ((uint8_t)0x09)
#define VGA_CRTC_CURSOR_START    ((uint8_t)0x0A)
#define VGA_CRTC_CURSOR_END      ((uint8_t)0x0B)
#define VGA_CRTC_CURSOR_LOC_HI   ((uint8_t)0x0E)
#define VGA_CRTC_CURSOR_LOC_LO   ((uint8_t)0x0F)
#define VGA_CRTC_CURSOR_START_CD (1 << 5)
/// @}

static uint16_t *const gp_vga_buf = (uint16_t *)VGA_MMIO_START;
static uint16_t
    gp_vga_shadow_buf[VGA_NUM_ROWS * VGA_NUM_COLS * VGA_NUM_SHADOW_SCREENS];
static size_t g_vga_start_at_sh_row;

static void prv_vga_enable_cursor(void);
static void prv_vga_disable_cursor(void);

static bool prv_vga_get_buf_idx(size_t lss_row, size_t lss_col,
                                size_t *p_vga_idx);
static void prv_vga_get_buf_range(size_t lss_start_row, size_t lss_num_rows,
                                  size_t *p_vga_start_row,
                                  size_t *p_vga_num_rows);
static void prv_fb_copy_sh_to_real(void);

void vga_init(void) {
    prv_vga_enable_cursor();
}

void vga_map_iomem(void) {
    for (uint32_t page = VGA_MMIO_START; page < VGA_MMIO_END; page += 4096) {
        vmm_map_kernel_page(page, page);
    }
}

size_t vga_height_chars(void) {
    return VGA_NUM_ROWS;
}

size_t vga_width_chars(void) {
    return VGA_NUM_COLS;
}

void vga_put_char_at(size_t row, size_t col, char ch) {
    ASSERT(row < VGA_NUM_ROWS && col < VGA_NUM_COLS);

    size_t sh_idx =
        ((VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS + row) * VGA_NUM_COLS +
        col;
    gp_vga_shadow_buf[sh_idx] = (0x0F << 8) | ch;

    size_t vga_idx;
    bool b_visible = prv_vga_get_buf_idx(row, col, &vga_idx);
    if (b_visible) { gp_vga_buf[vga_idx] = (0x0F << 8) | ch; }
}

void vga_put_cursor_at(size_t lss_row, size_t lss_col) {
    ASSERT(lss_row < VGA_NUM_ROWS && lss_col < VGA_NUM_COLS);

    size_t vga_idx = lss_row * VGA_NUM_COLS + lss_col;
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_LOC_HI);
    port_outb(VGA_PORT_CRTC_DATA, ((uint8_t)(vga_idx >> 8)));
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_LOC_LO);
    port_outb(VGA_PORT_CRTC_DATA, ((uint8_t)vga_idx));
}

void vga_clear_rows(size_t lss_start_row, size_t lss_num_rows) {
    size_t sh_start_row =
        (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS + lss_start_row;
    size_t sh_num_words = lss_num_rows * VGA_NUM_COLS;
    kmemset_word(&gp_vga_shadow_buf[sh_start_row * VGA_NUM_COLS], 0x0F << 8,
                 sh_num_words);

    size_t vga_start_row;
    size_t vga_num_rows;
    prv_vga_get_buf_range(lss_start_row, lss_num_rows, &vga_start_row,
                          &vga_num_rows);
    size_t vga_num_words = vga_num_rows * VGA_NUM_COLS;
    kmemset_word(&gp_vga_buf[vga_start_row * VGA_NUM_COLS], 0x0F << 8,
                 vga_num_words);
}

void vga_scroll_new_row(void) {
    // Move every shadow row except the first one up.
    kmemmove(gp_vga_shadow_buf, &gp_vga_shadow_buf[1 * VGA_NUM_COLS],
             (VGA_NUM_SHADOW_SCREENS * VGA_NUM_ROWS - 1) * VGA_PITCH);

    // Clear the last shadow row.
    size_t sh_last_row = VGA_NUM_SHADOW_SCREENS * VGA_NUM_ROWS - 1;
    kmemset_word(&gp_vga_shadow_buf[sh_last_row * VGA_NUM_COLS], 0x0F << 8,
                 VGA_NUM_COLS);

    prv_fb_copy_sh_to_real();
}

void vga_init_history(void) {
    g_vga_start_at_sh_row = (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS;
}

void vga_clear_history(void) {
    kmemset_word(gp_vga_shadow_buf, 0x0F << 8,
                 VGA_NUM_SHADOW_SCREENS * VGA_NUM_ROWS * VGA_NUM_COLS);
    g_vga_start_at_sh_row = (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS;
}

size_t vga_history_screens(void) {
    return VGA_NUM_SHADOW_SCREENS;
}

size_t vga_history_pos(void) {
    return g_vga_start_at_sh_row;
}

void vga_set_history_mode(size_t row_from_start) {
    // Assert that there are enough rows until the shadow buffer end to fill up
    // the visible screen.
    ASSERT(row_from_start <= (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS);

    g_vga_start_at_sh_row = row_from_start;
    prv_fb_copy_sh_to_real();

    if (g_vga_start_at_sh_row < (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS) {
        prv_vga_disable_cursor();
    } else {
        prv_vga_enable_cursor();
    }
}

bool vga_is_history_mode_active(void) {
    return g_vga_start_at_sh_row < (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS;
}

static void prv_vga_enable_cursor(void) {
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_MAX_SCAN_LINE);
    uint8_t max_scan_line = (port_inb(VGA_PORT_CRTC_DATA) & 0x1F);

    uint8_t start = 1;
    uint8_t end = (max_scan_line - 1);

    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_START);
    port_outb(VGA_PORT_CRTC_DATA,
              ((port_inb(VGA_PORT_CRTC_DATA) & 0xC0) | start));
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_END);
    port_outb(VGA_PORT_CRTC_DATA,
              ((port_inb(VGA_PORT_CRTC_DATA) & 0xC0) | end));
}

static void prv_vga_disable_cursor(void) {
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_START);
    port_outb(VGA_PORT_CRTC_DATA, VGA_CRTC_CURSOR_START_CD);
}

/**
 * Gets the framebuffer offset for a character on the last shadow screen.
 *
 * @returns `true` if the position is visible and @a *p_vga_idx has been set.
 */
static bool prv_vga_get_buf_idx(size_t lss_row, size_t lss_col,
                                size_t *p_vga_idx) {
    /*
     * Explanation by example:
     *
     * The shadow buffer has 3 screens, or 75 rows (VGA_NUM_ROWS * 3). The last
     * shadow screen starts at row 50 and ends at row 75 exclusively. Let's say
     * the user-visible part begins at row 49 (g_vga_start_at_sh_row = 49) and
     * ends at +VGA_NUM_ROWS = 74 exclusively. Every row of the last shadow
     * screen is visible, except for the last one (row 74). This can be
     * expressed as:
     *
     *   bool is_visible = lss_row >= 49 && lss_row < 74
     *
     * So, the visible part of the history buffer consists of:
     * - the last row of the second-to-last shadow screen (row 49), and
     * - rows [50, 73] of the last shadow screen.
     */

    size_t sh_row = (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS + lss_row;
    if (sh_row >= g_vga_start_at_sh_row) {
        size_t vga_row = sh_row - g_vga_start_at_sh_row;
        *p_vga_idx = vga_row * VGA_NUM_COLS + lss_col;
        return true;
    } else {
        return false;
    }
}

/**
 * Gets the framebuffer row range for a range of rows on the last shadow buffer.
 *
 * Similar to #prv_vga_get_buf_idx(), except this one converts the range of rows
 * on the last shadow screen to their corresponding visible rows.
 */
static void prv_vga_get_buf_range(size_t lss_start_row, size_t lss_num_rows,
                                  size_t *p_vga_start_row,
                                  size_t *p_vga_num_rows) {
    size_t sh_start_row =
        (VGA_NUM_SHADOW_SCREENS - 1) * VGA_NUM_ROWS + lss_start_row;
    size_t sh_end_row = sh_start_row + lss_num_rows;

    if (sh_start_row >= g_vga_start_at_sh_row) {
        *p_vga_start_row = sh_start_row - g_vga_start_at_sh_row;

        if (sh_end_row > g_vga_start_at_sh_row + VGA_NUM_ROWS) {
            *p_vga_num_rows =
                lss_num_rows -
                (sh_end_row - (g_vga_start_at_sh_row + VGA_NUM_ROWS));
        } else {
            *p_vga_num_rows = lss_num_rows;
        }
    } else {
        *p_vga_start_row = 0;
        *p_vga_num_rows = 0;
    }
}

static void prv_fb_copy_sh_to_real(void) {
    kmemcpy(gp_vga_buf,
            &gp_vga_shadow_buf[g_vga_start_at_sh_row * VGA_NUM_COLS],
            VGA_NUM_ROWS * VGA_PITCH);
}
