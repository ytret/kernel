#include <stddef.h>
#include <stdint.h>

#include "kbd.h"
#include "kprintf.h"
#include "panic.h"
#include "pic.h"
#include "port.h"

#define PORT_DATA   0x0060
#define PORT_CMD    0x0064
#define PORT_STATUS 0x0064

#define CODE_BUF_SIZE  10
#define EVENT_BUF_SIZE 10

// Buffer to store scancodes before they are parsed.
static volatile uint8_t gp_code_buf[CODE_BUF_SIZE];
static volatile size_t g_code_buf_pos;

static void (*gp_event_callback)(uint8_t key, bool b_released);

static uint8_t read_code(void);
static void append_code(uint8_t sc);
static void try_parse_codes(void);
static void new_event(uint8_t key, bool b_released);

void kbd_set_callback(void (*p_callback)(uint8_t key, bool b_released)) {
    gp_event_callback = p_callback;
}

void kbd_irq_handler(void) {
    uint8_t sc = read_code();
    append_code(sc);
    try_parse_codes();

    pic_send_eoi(1);
}

static uint8_t read_code(void) {
    return port_inb(PORT_DATA);
}

static void append_code(uint8_t sc) {
    if (g_code_buf_pos >= CODE_BUF_SIZE) {
        kprintf("kbd: append_code: scancode buffer is full\n");
        panic("buffer overflow");
    }

    gp_code_buf[g_code_buf_pos] = sc;
    g_code_buf_pos++;
}

static void try_parse_codes(void) {
    size_t num_codes = g_code_buf_pos;
    bool b_parsed = false;

    // Result of parsing.
    uint8_t key;
    bool b_released;

    if (0 == num_codes) {
        return;
    } else if (1 == num_codes) {
        b_parsed = true;

        uint8_t key_code = gp_code_buf[0];
        b_released = false;

        if ((0x81 <= key_code) && (key_code <= 0xD8)) {
            // FIXME: figure out whether each 'key pressed' scancode listed
            // below indeed has a 'key released' counterpart greater by 0x80.
            key_code -= 0x80;
            b_released = true;
        }

        switch (key_code) {
        case 0x01:
            key = KEY_ESCAPE;
            break;
        case 0x29:
            key = KEY_BACKTICK;
            break;
        case 0x0F:
            key = KEY_TAB;
            break;
        case 0x3A:
            key = KEY_CAPSLOCK;
            break;
        case 0x2A:
            key = KEY_LSHIFT;
            break;
        case 0x36:
            key = KEY_RSHIFT;
            break;
        case 0x1D:
            key = KEY_LCTRL;
            break;
        case 0x38:
            key = KEY_LALT;
            break;
        case 0x39:
            key = KEY_SPACE;
            break;

        case 0x3B:
            key = KEY_F1;
            break;
        case 0x3C:
            key = KEY_F2;
            break;
        case 0x3D:
            key = KEY_F3;
            break;
        case 0x3E:
            key = KEY_F4;
            break;
        case 0x3F:
            key = KEY_F5;
            break;
        case 0x40:
            key = KEY_F6;
            break;
        case 0x41:
            key = KEY_F7;
            break;
        case 0x42:
            key = KEY_F8;
            break;
        case 0x43:
            key = KEY_F9;
            break;
        case 0x44:
            key = KEY_F10;
            break;
        case 0x57:
            key = KEY_F11;
            break;
        case 0x58:
            key = KEY_F12;
            break;

        case 0x45:
            key = KEY_NUMLOCK;
            break;
        case 0x46:
            key = KEY_SCROLLLOCK;
            break;

        case 0x02:
            key = KEY_1;
            break;
        case 0x03:
            key = KEY_2;
            break;
        case 0x04:
            key = KEY_3;
            break;
        case 0x05:
            key = KEY_4;
            break;
        case 0x06:
            key = KEY_5;
            break;
        case 0x07:
            key = KEY_6;
            break;
        case 0x08:
            key = KEY_7;
            break;
        case 0x09:
            key = KEY_8;
            break;
        case 0x0A:
            key = KEY_9;
            break;
        case 0x0B:
            key = KEY_0;
            break;

        case 0x0C:
            key = KEY_MINUS;
            break;
        case 0x0D:
            key = KEY_EQUALS;
            break;
        case 0x0E:
            key = KEY_BACKSPACE;
            break;

        case 0x10:
            key = KEY_Q;
            break;
        case 0x11:
            key = KEY_W;
            break;
        case 0x12:
            key = KEY_E;
            break;
        case 0x13:
            key = KEY_R;
            break;
        case 0x14:
            key = KEY_T;
            break;
        case 0x15:
            key = KEY_Y;
            break;
        case 0x16:
            key = KEY_U;
            break;
        case 0x17:
            key = KEY_I;
            break;
        case 0x18:
            key = KEY_O;
            break;
        case 0x19:
            key = KEY_P;
            break;
        case 0x1A:
            key = KEY_LBRACKET;
            break;
        case 0x1B:
            key = KEY_RBRACKET;
            break;
        case 0x2B:
            key = KEY_BACKSLASH;
            break;
        case 0x1E:
            key = KEY_A;
            break;
        case 0x1F:
            key = KEY_S;
            break;
        case 0x20:
            key = KEY_D;
            break;
        case 0x21:
            key = KEY_F;
            break;
        case 0x22:
            key = KEY_G;
            break;
        case 0x23:
            key = KEY_H;
            break;
        case 0x24:
            key = KEY_J;
            break;
        case 0x25:
            key = KEY_K;
            break;
        case 0x26:
            key = KEY_L;
            break;
        case 0x27:
            key = KEY_SEMICOLON;
            break;
        case 0x28:
            key = KEY_APOSTROPHE;
            break;
        case 0x1C:
            key = KEY_ENTER;
            break;
        case 0x2C:
            key = KEY_Z;
            break;
        case 0x2D:
            key = KEY_X;
            break;
        case 0x2E:
            key = KEY_C;
            break;
        case 0x2F:
            key = KEY_V;
            break;
        case 0x30:
            key = KEY_B;
            break;
        case 0x31:
            key = KEY_N;
            break;
        case 0x32:
            key = KEY_M;
            break;
        case 0x33:
            key = KEY_COMMA;
            break;
        case 0x34:
            key = KEY_PERIOD;
            break;
        case 0x35:
            key = KEY_SLASH;
            break;

        case 0x37:
            key = KEY_NPASTERISK;
            break;
        case 0x4A:
            key = KEY_NPMINUS;
            break;
        case 0x4E:
            key = KEY_NPPLUS;
            break;
        case 0x53:
            key = KEY_NPPERIOD;
            break;

        case 0x4F:
            key = KEY_NP1;
            break;
        case 0x50:
            key = KEY_NP2;
            break;
        case 0x51:
            key = KEY_NP3;
            break;
        case 0x4B:
            key = KEY_NP4;
            break;
        case 0x4C:
            key = KEY_NP5;
            break;
        case 0x4D:
            key = KEY_NP6;
            break;
        case 0x47:
            key = KEY_NP7;
            break;
        case 0x48:
            key = KEY_NP8;
            break;
        case 0x49:
            key = KEY_NP9;
            break;
        case 0x52:
            key = KEY_NP0;
            break;

        default:
            return;
        }

    } else if ((2 == num_codes) && (0xE0 == gp_code_buf[0])) {
        b_parsed = true;

        uint8_t key_code = gp_code_buf[1];
        b_released = false;

        if ((0x99 <= key_code) && (key_code <= 0xED)) {
            key_code -= 0x80;
            b_released = true;
        }

        switch (key_code) {
        case 0x1D:
            key = KEY_RCTRL;
            break;
        case 0x38:
            key = KEY_RALT;
            break;
        case 0x5D:
            key = KEY_MENU;
            break;
        case 0x5B:
            key = KEY_SUPER;
            break;

        case 0x52:
            key = KEY_INSERT;
            break;
        case 0x53:
            key = KEY_DELETE;
            break;

        case 0x47:
            key = KEY_HOME;
            break;
        case 0x4F:
            key = KEY_END;
            break;
        case 0x49:
            key = KEY_PAGEUP;
            break;
        case 0x51:
            key = KEY_PAGEDOWN;
            break;

        case 0x4B:
            key = KEY_LEFTARROW;
            break;
        case 0x48:
            key = KEY_UPARROW;
            break;
        case 0x50:
            key = KEY_DOWNARROW;
            break;
        case 0x4D:
            key = KEY_RIGHTARROW;
            break;

        case 0x35:
            key = KEY_NPSLASH;
            break;
        case 0x1C:
            key = KEY_NPENTER;
            break;

        default:
            return;
        }
    } else if (4 == num_codes) {
        if ((0xE0 == gp_code_buf[0]) && (0xE0 == gp_code_buf[2])) {
            if ((0x2A == gp_code_buf[1]) && (0x37 == gp_code_buf[3])) {
                b_parsed = true;
                key = KEY_PRINTSCREEN;
                b_released = false;
            } else if ((0xB7 == gp_code_buf[1]) && (0xAA == gp_code_buf[3])) {
                b_parsed = true;
                key = KEY_PRINTSCREEN;
                b_released = true;
            }
        }
    } else if (6 == num_codes) {
        if ((0xE1 == gp_code_buf[0]) && (0x1D == gp_code_buf[1]) &&
            (0x45 == gp_code_buf[2]) && (0xE1 == gp_code_buf[3]) &&
            (0x9D == gp_code_buf[4]) && (0xC5 == gp_code_buf[5])) {
            b_parsed = true;
            key = KEY_PAUSEBREAK;
            b_released = false;
        }
    } else if (num_codes > 6) {
        kprintf("kbd: discarding unknown sequence: ");
        for (size_t idx = 0; idx < num_codes; idx++) {
            kprintf("%x ", gp_code_buf[idx]);
        }
        kprintf("\n");

        // Reset the scancode buffer.
        g_code_buf_pos = 0;
    }

    if (b_parsed) {
        // Reset the scancode buffer.
        g_code_buf_pos = 0;

        // Let the callback parse the event.
        new_event(key, b_released);
    }
}

static void new_event(uint8_t key, bool b_released) {
    if (gp_event_callback) { gp_event_callback(key, b_released); }
}
