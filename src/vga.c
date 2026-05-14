/**
 * @file vga.c
 * VGA text-mode terminal implementation.
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

static const textdisp_ops_t g_vga_disp_ops = {
    .p_init = NULL,
    .p_map_iomem = vga_map_iomem,
    .p_put_char_at = vga_put_char_at,
    .p_put_cursor_at = vga_put_cursor_at,
    .p_clear_rows = vga_clear_rows,
    .p_scroll_new_row = vga_scroll_new_row,
};

static void prv_vga_enable_cursor(void);
static void prv_vga_disable_cursor(void);

const textdisp_ops_t *vga_textdisp_ops(void) {
    return &g_vga_disp_ops;
}

void vga_early_init(void) {
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

    const size_t sh_idx = col + VGA_NUM_COLS * row;
    gp_vga_buf[sh_idx] = (0x0F << 8) | ch;
}

void vga_put_cursor_at(size_t row, size_t col) {
    ASSERT(row < VGA_NUM_ROWS && col < VGA_NUM_COLS);

    const size_t vga_idx = col + row * VGA_NUM_COLS;
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_LOC_HI);
    port_outb(VGA_PORT_CRTC_DATA, ((uint8_t)(vga_idx >> 8)));
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_LOC_LO);
    port_outb(VGA_PORT_CRTC_DATA, ((uint8_t)vga_idx));
}

void vga_clear_rows(size_t start_row, size_t num_rows) {
    const size_t num_words = num_rows * VGA_NUM_COLS;
    kmemset_word(&gp_vga_buf[start_row * VGA_NUM_COLS], 0x0F << 8, num_words);
}

void vga_scroll_new_row(void) {
    kmemmove(gp_vga_buf, &gp_vga_buf[VGA_NUM_COLS * 1],
             VGA_PITCH * (VGA_NUM_ROWS - 1));
    kmemset_word(&gp_vga_buf[VGA_NUM_COLS * (VGA_NUM_ROWS - 1)], 0x0F << 8,
                 VGA_NUM_COLS);
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

[[gnu::unused]]
static void prv_vga_disable_cursor(void) {
    port_outb(VGA_PORT_CRTC_ADDR, VGA_CRTC_CURSOR_START);
    port_outb(VGA_PORT_CRTC_DATA, VGA_CRTC_CURSOR_START_CD);
}
