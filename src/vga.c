#include <stddef.h>
#include <stdint.h>

#include "assert.h"
#include "memfun.h"
#include "panic.h"
#include "port.h"
#include "vga.h"

#define VGA_MEMORY_ADDR 0xB8000

#define MAX_ROWS 25
#define MAX_COLS 80
#define PITCH    (MAX_COLS * 2)

#define PORT_CRTC_ADDR ((uint16_t)0x03D4)
#define PORT_CRTC_DATA ((uint16_t)0x03D5)

#define REG_CRTC_MAX_SCAN_LINE ((uint8_t)0x09)
#define REG_CRTC_CURSOR_START  ((uint8_t)0x0A)
#define REG_CRTC_CURSOR_END    ((uint8_t)0x0B)
#define REG_CRTC_CURSOR_LOC_HI ((uint8_t)0x0E)
#define REG_CRTC_CURSOR_LOC_LO ((uint8_t)0x0F)

#define HISTORY_SCREENS 2

static uint16_t *const gp_vga_memory = (uint16_t *)VGA_MEMORY_ADDR;

static uint16_t gp_history_buf[MAX_ROWS * MAX_COLS * HISTORY_SCREENS];
static size_t g_history_screens = HISTORY_SCREENS;
static size_t g_history_pos = (HISTORY_SCREENS - 1) * MAX_ROWS;

static void draw_from_history(void);

void vga_init(void) {
    // Set up the cursor.
    port_outb(PORT_CRTC_ADDR, REG_CRTC_MAX_SCAN_LINE);
    uint8_t max_scan_line = (port_inb(PORT_CRTC_DATA) & 0x1F);

    uint8_t start = 1;
    uint8_t end = (max_scan_line - 1);

    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_START);
    port_outb(PORT_CRTC_DATA, ((port_inb(PORT_CRTC_DATA) & 0xC0) | start));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_END);
    port_outb(PORT_CRTC_DATA, ((port_inb(PORT_CRTC_DATA) & 0xC0) | end));
}

size_t vga_height_chars(void) {
    return MAX_ROWS;
}

size_t vga_width_chars(void) {
    return MAX_COLS;
}

void vga_put_char_at(size_t row, size_t col, char ch) {
    if ((row >= MAX_ROWS) || (col >= MAX_COLS)) { panic_silent(); }

    size_t idx = row * MAX_COLS + col;
    size_t hist_idx = (g_history_pos + row) * MAX_COLS + col;
    gp_vga_memory[idx] = (0x0F << 8) | ch;
    gp_history_buf[hist_idx] = (0x0F << 8) | ch;
}

void vga_put_cursor_at(size_t row, size_t col) {
    if ((row >= MAX_ROWS) || (col >= MAX_COLS)) { panic_silent(); }

    size_t char_idx = (row * MAX_COLS + col);
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_HI);
    port_outb(PORT_CRTC_DATA, ((uint8_t)(char_idx >> 8)));
    port_outb(PORT_CRTC_ADDR, REG_CRTC_CURSOR_LOC_LO);
    port_outb(PORT_CRTC_DATA, ((uint8_t)char_idx));
}

void vga_clear_rows(size_t start_row, size_t num_rows) {
    size_t start_offset = start_row * MAX_COLS;
    size_t num_words = num_rows * MAX_COLS;
    memset_word(&gp_vga_memory[start_offset], 0x0F << 8, num_words);
    memset_word(&gp_history_buf[(g_history_pos + start_row) * MAX_COLS],
                0x0F << 8, num_words);
}

void vga_scroll_new_row(void) {
    // Bury the first row.
    __builtin_memmove(gp_vga_memory, &gp_vga_memory[1 * MAX_COLS],
                      (2 * (MAX_ROWS - 1) * MAX_COLS));
    __builtin_memmove(gp_history_buf, &gp_history_buf[1 * MAX_COLS],
                      (2 * (MAX_ROWS * g_history_screens - 1) * MAX_COLS));

    // Clean the last row.
    for (size_t col = 0; col < MAX_COLS; col++) {
        vga_put_char_at((MAX_ROWS - 1), col, ' ');
    }
}

void vga_clear_history(void) {
    memset_word(gp_history_buf, 0x0F << 8,
                MAX_ROWS * MAX_COLS * g_history_screens);
    g_history_pos = (g_history_screens - 1) * MAX_ROWS;
}

size_t vga_history_screens(void) {
    return g_history_screens;
}

size_t vga_history_pos(void) {
    return g_history_pos;
}

void vga_set_history_pos(size_t row_from_start) {
    ASSERT(row_from_start <= (g_history_screens - 1) * MAX_ROWS);
    g_history_pos = row_from_start;
    draw_from_history();
}

static void draw_from_history(void) {
    __builtin_memcpy(gp_vga_memory, &gp_history_buf[g_history_pos * MAX_COLS],
                     MAX_ROWS * PITCH);
}
