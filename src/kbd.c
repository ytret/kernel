#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stddef.h>
#include <stdint.h>

#include "conmgr.h"
#include "inputmgr.h"
#include "kbd.h"
#include "keymap.h"
#include "log.h"
#include "panic.h"
#include "port.h"

#include "arch/x86/apic/lapic.h"

#define KBD_PORT_DATA   0x0060
#define KBD_PORT_CMD    0x0064
#define KBD_PORT_STATUS 0x0064

#define KBD_CODE_BUF_SIZE 10

typedef struct {
    /**
     * Buffer of incomplete scancode sequences.
     */
    volatile uint8_t code_buf[KBD_CODE_BUF_SIZE];
    volatile size_t code_buf_pos;
} kbd_t;

static kbd_t g_kbd;

static void prv_kbd_append_code(kbd_t *kbd, uint8_t sc);
static bool prv_kbd_try_parse_codes(kbd_t *kbd, kbd_event_t *event);

static void prv_kbd_write(uint8_t key, bool b_released);

static uint8_t prv_kbd_read_code(void);

void kbd_init(void) {
    keymap_init();

    // On QEMU this is required to free space in the buffer.
    prv_kbd_read_code();
}

void kbd_irq_handler(void) {
    const uint8_t sc = prv_kbd_read_code();
    prv_kbd_append_code(&g_kbd, sc);

    kbd_event_t event;
    if (prv_kbd_try_parse_codes(&g_kbd, &event)) {
        prv_kbd_write(event.key, event.b_released);
    }

    lapic_send_eoi();
}

static void prv_kbd_append_code(kbd_t *kbd, uint8_t sc) {
    if (kbd->code_buf_pos >= KBD_CODE_BUF_SIZE) {
        PANIC("scancode buffer is full");
    }

    kbd->code_buf[kbd->code_buf_pos] = sc;
    kbd->code_buf_pos++;
}

static bool prv_kbd_try_parse_codes(kbd_t *kbd, kbd_event_t *event) {
    const size_t num_codes = kbd->code_buf_pos;

    bool b_parsed = false;
    uint8_t key;
    bool b_released;

    if (num_codes == 0) {
        return false;
    } else if (num_codes == 1) {
        uint8_t key_code = kbd->code_buf[0];
        b_released = false;

        if (0x81 <= key_code && key_code <= 0xD8) {
            // FIXME: figure out whether each 'key pressed' scancode listed
            // below indeed has a 'key released' counterpart greater by 0x80.
            key_code -= 0x80;
            b_released = true;
        }

        switch (key_code) {
        case 0x01: key = KEY_ESCAPE; break;
        case 0x29: key = KEY_BACKTICK; break;
        case 0x0F: key = KEY_TAB; break;
        case 0x3A: key = KEY_CAPSLOCK; break;
        case 0x2A: key = KEY_LSHIFT; break;
        case 0x36: key = KEY_RSHIFT; break;
        case 0x1D: key = KEY_LCTRL; break;
        case 0x38: key = KEY_LALT; break;
        case 0x39: key = KEY_SPACE; break;

        case 0x3B: key = KEY_F1; break;
        case 0x3C: key = KEY_F2; break;
        case 0x3D: key = KEY_F3; break;
        case 0x3E: key = KEY_F4; break;
        case 0x3F: key = KEY_F5; break;
        case 0x40: key = KEY_F6; break;
        case 0x41: key = KEY_F7; break;
        case 0x42: key = KEY_F8; break;
        case 0x43: key = KEY_F9; break;
        case 0x44: key = KEY_F10; break;
        case 0x57: key = KEY_F11; break;
        case 0x58: key = KEY_F12; break;

        case 0x45: key = KEY_NUMLOCK; break;
        case 0x46: key = KEY_SCROLLLOCK; break;

        case 0x02: key = KEY_1; break;
        case 0x03: key = KEY_2; break;
        case 0x04: key = KEY_3; break;
        case 0x05: key = KEY_4; break;
        case 0x06: key = KEY_5; break;
        case 0x07: key = KEY_6; break;
        case 0x08: key = KEY_7; break;
        case 0x09: key = KEY_8; break;
        case 0x0A: key = KEY_9; break;
        case 0x0B: key = KEY_0; break;

        case 0x0C: key = KEY_MINUS; break;
        case 0x0D: key = KEY_EQUALS; break;
        case 0x0E: key = KEY_BACKSPACE; break;

        case 0x10: key = KEY_Q; break;
        case 0x11: key = KEY_W; break;
        case 0x12: key = KEY_E; break;
        case 0x13: key = KEY_R; break;
        case 0x14: key = KEY_T; break;
        case 0x15: key = KEY_Y; break;
        case 0x16: key = KEY_U; break;
        case 0x17: key = KEY_I; break;
        case 0x18: key = KEY_O; break;
        case 0x19: key = KEY_P; break;
        case 0x1A: key = KEY_LBRACKET; break;
        case 0x1B: key = KEY_RBRACKET; break;
        case 0x2B: key = KEY_BACKSLASH; break;
        case 0x1E: key = KEY_A; break;
        case 0x1F: key = KEY_S; break;
        case 0x20: key = KEY_D; break;
        case 0x21: key = KEY_F; break;
        case 0x22: key = KEY_G; break;
        case 0x23: key = KEY_H; break;
        case 0x24: key = KEY_J; break;
        case 0x25: key = KEY_K; break;
        case 0x26: key = KEY_L; break;
        case 0x27: key = KEY_SEMICOLON; break;
        case 0x28: key = KEY_APOSTROPHE; break;
        case 0x1C: key = KEY_ENTER; break;
        case 0x2C: key = KEY_Z; break;
        case 0x2D: key = KEY_X; break;
        case 0x2E: key = KEY_C; break;
        case 0x2F: key = KEY_V; break;
        case 0x30: key = KEY_B; break;
        case 0x31: key = KEY_N; break;
        case 0x32: key = KEY_M; break;
        case 0x33: key = KEY_COMMA; break;
        case 0x34: key = KEY_PERIOD; break;
        case 0x35: key = KEY_SLASH; break;

        case 0x37: key = KEY_NPASTERISK; break;
        case 0x4A: key = KEY_NPMINUS; break;
        case 0x4E: key = KEY_NPPLUS; break;
        case 0x53: key = KEY_NPPERIOD; break;

        case 0x4F: key = KEY_NP1; break;
        case 0x50: key = KEY_NP2; break;
        case 0x51: key = KEY_NP3; break;
        case 0x4B: key = KEY_NP4; break;
        case 0x4C: key = KEY_NP5; break;
        case 0x4D: key = KEY_NP6; break;
        case 0x47: key = KEY_NP7; break;
        case 0x48: key = KEY_NP8; break;
        case 0x49: key = KEY_NP9; break;
        case 0x52: key = KEY_NP0; break;

        default:   return false;
        }

        b_parsed = true;
    } else if (num_codes == 2 && kbd->code_buf[0] == 0xE0) {
        uint8_t key_code = kbd->code_buf[1];
        b_released = false;

        if ((0x99 <= key_code) && (key_code <= 0xED)) {
            key_code -= 0x80;
            b_released = true;
        }

        switch (key_code) {
        case 0x1D: key = KEY_RCTRL; break;
        case 0x38: key = KEY_RALT; break;
        case 0x5D: key = KEY_MENU; break;
        case 0x5B: key = KEY_SUPER; break;

        case 0x52: key = KEY_INSERT; break;
        case 0x53: key = KEY_DELETE; break;

        case 0x47: key = KEY_HOME; break;
        case 0x4F: key = KEY_END; break;
        case 0x49: key = KEY_PAGEUP; break;
        case 0x51: key = KEY_PAGEDOWN; break;

        case 0x4B: key = KEY_LEFTARROW; break;
        case 0x48: key = KEY_UPARROW; break;
        case 0x50: key = KEY_DOWNARROW; break;
        case 0x4D: key = KEY_RIGHTARROW; break;

        case 0x35: key = KEY_NPSLASH; break;
        case 0x1C: key = KEY_NPENTER; break;

        default:   return false;
        }

        b_parsed = true;
    } else if (num_codes == 4) {
        if ((0xE0 == kbd->code_buf[0]) && (0xE0 == kbd->code_buf[2])) {
            if ((0x2A == kbd->code_buf[1]) && (0x37 == kbd->code_buf[3])) {
                key = KEY_PRINTSCREEN;
                b_released = false;
                b_parsed = true;
            } else if ((0xB7 == kbd->code_buf[1]) &&
                       (0xAA == kbd->code_buf[3])) {
                key = KEY_PRINTSCREEN;
                b_released = true;
                b_parsed = true;
            }
        }
    } else if (6 == num_codes) {
        if ((0xE1 == kbd->code_buf[0]) && (0x1D == kbd->code_buf[1]) &&
            (0x45 == kbd->code_buf[2]) && (0xE1 == kbd->code_buf[3]) &&
            (0x9D == kbd->code_buf[4]) && (0xC5 == kbd->code_buf[5])) {
            key = KEY_PAUSEBREAK;
            b_released = false;
            b_parsed = true;
        }
    } else if (num_codes > 6) {
        LOG_DEBUG("discarding unknown sequence:");
        for (size_t idx = 0; idx < num_codes; idx++) {
            LOG_DEBUG("%x ", kbd->code_buf[idx]);
        }

        kbd->code_buf_pos = 0;
    }

    if (b_parsed) {
        kbd->code_buf_pos = 0;

        event->key = key;
        event->b_released = b_released;
    }

    return b_parsed;
}

static void prv_kbd_write(uint8_t key, bool b_released) {
    // Alt-number keys switch consoles (Alt-1 = boot console, Alt-2 = console 1,
    // ..., Alt-0 = console 9).
    if (!b_released && keymap_is_alt_pressed()) {
        size_t idx;
        switch (key) {
        case KEY_1: idx = 0; break;
        case KEY_2: idx = 1; break;
        case KEY_3: idx = 2; break;
        case KEY_4: idx = 3; break;
        case KEY_5: idx = 4; break;
        case KEY_6: idx = 5; break;
        case KEY_7: idx = 6; break;
        case KEY_8: idx = 7; break;
        case KEY_9: idx = 8; break;
        case KEY_0: idx = 9; break;
        default:    goto normal;
        }
        conmgr_switch(idx);
        return;
    }

normal:
    char seq_buf[8];
    const size_t seq_buf_size = sizeof(seq_buf);
    const kbd_event_t event = {
        .key = key,
        .b_released = b_released,
    };
    const size_t seq_len = keymap_process(&event, seq_buf, seq_buf_size);

    if (inputmgr_write(seq_buf, seq_len) == 0) {
        LOG_FLOW("ignored key event %u released %d", key, b_released);
    }
}

static uint8_t prv_kbd_read_code(void) {
    return port_inb(KBD_PORT_DATA);
}
