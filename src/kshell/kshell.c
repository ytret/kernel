#include "kbd.h"
#include "kprintf.h"
#include "kshell/kshcmd/kshcmd.h"
#include "kshell/kshell.h"
#include "panic.h"
#include "term.h"

#define CMD_BUF_SIZE 256

// Command buffer.
static volatile char gp_cmd_buf[CMD_BUF_SIZE];
static volatile size_t g_cmd_buf_pos;

// Keyboard state.
static volatile bool gb_shifted;

// Input reading functions.
static char const *read_cmd(void);

// Command buffer functions.
static void buf_append(char ch);
static void buf_remove(void);
static volatile char const *buf_get_cmd(void);

// Keyboard-related functions.
static bool parse_kbd_event(kbd_event_t *p_event);
static void echo_key(uint8_t key);
static char char_from_key(uint8_t key);

void kshell(void) {
    for (;;) {
        kprintf("> ");
        char const *p_cmd = read_cmd();
        kshcmd_parse(p_cmd);
    }
}

/*
 * Synchronously reads a command from the user and returns it.
 *
 * NOTE: each call invalidates the pointer returned by the previous call.
 */
static char const *read_cmd(void) {
    bool b_enter = false;
    while (!b_enter) {
        kbd_event_t event;
        term_read_kbd_event(&event);
        b_enter = parse_kbd_event(&event);
    }

    return (char const *)buf_get_cmd();
}

static void buf_append(char ch) {
    if (g_cmd_buf_pos >= (CMD_BUF_SIZE - 1)) { return; }

    gp_cmd_buf[g_cmd_buf_pos] = ch;
    g_cmd_buf_pos++;
}

static void buf_remove(void) {
    if (g_cmd_buf_pos > 0) { g_cmd_buf_pos--; }
}

/*
 * Returns a NUL-terminated command string buffer and resets the buffer
 * position.
 */
static volatile char const *buf_get_cmd(void) {
    if (g_cmd_buf_pos >= CMD_BUF_SIZE) {
        panic_enter();
        kprintf("kshell: command buffer overflow\n");
        panic("buffer overflow");
    }

    gp_cmd_buf[g_cmd_buf_pos] = 0;
    g_cmd_buf_pos = 0;
    return gp_cmd_buf;
}

static bool parse_kbd_event(kbd_event_t *p_event) {
    if (p_event->key == KEY_LSHIFT || p_event->key == KEY_RSHIFT) {
        gb_shifted = !p_event->b_released;
    }

    // Henceforth parse only 'key pressed' events.
    if (p_event->b_released) { return false; }

    if (p_event->key == KEY_BACKSPACE) {
        // Allow to delete as many characters, as there are in the command
        // buffer.
        if (g_cmd_buf_pos == 0) { return false; }

        term_acquire_mutex();
        size_t row = term_row();
        size_t col = term_col();

        if ((row == 0) && (col == 0)) {
            term_release_mutex();
            return false;
        }

        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = (term_width() - 1);
        }

        term_put_char_at(row, col, ' ');
        term_put_cursor_at(row, col);
        term_release_mutex();

        // Remove one char from the buffer.
        buf_remove();
    } else if (p_event->key == KEY_ENTER) {
        echo_key(p_event->key);
        return true;
    } else {
        // Print the key and add it to the buffer, if it's a char.
        echo_key(p_event->key);

        char ch = char_from_key(p_event->key);
        if (ch) { buf_append(ch); }
    }

    return false;
}

static void echo_key(uint8_t key) {
    char p_echoed[2] = {0};
    p_echoed[0] = char_from_key(key);
    if (p_echoed[0]) { kprintf("%s", p_echoed); }
}

static char char_from_key(uint8_t key) {
    switch (key) {
    case KEY_BACKTICK:
        return gb_shifted ? '~' : '`';
    case KEY_1:
        return gb_shifted ? '!' : '1';
    case KEY_2:
        return gb_shifted ? '@' : '2';
    case KEY_3:
        return gb_shifted ? '#' : '3';
    case KEY_4:
        return gb_shifted ? '$' : '4';
    case KEY_5:
        return gb_shifted ? '%' : '5';
    case KEY_6:
        return gb_shifted ? '^' : '6';
    case KEY_7:
        return gb_shifted ? '&' : '7';
    case KEY_8:
        return gb_shifted ? '*' : '8';
    case KEY_9:
        return gb_shifted ? '(' : '9';
    case KEY_0:
        return gb_shifted ? ')' : '0';
    case KEY_MINUS:
        return gb_shifted ? '_' : '-';
    case KEY_EQUALS:
        return gb_shifted ? '+' : '=';
    case KEY_Q:
        return gb_shifted ? 'Q' : 'q';
    case KEY_W:
        return gb_shifted ? 'W' : 'w';
    case KEY_E:
        return gb_shifted ? 'E' : 'e';
    case KEY_R:
        return gb_shifted ? 'R' : 'r';
    case KEY_T:
        return gb_shifted ? 'T' : 't';
    case KEY_Y:
        return gb_shifted ? 'Y' : 'y';
    case KEY_U:
        return gb_shifted ? 'U' : 'u';
    case KEY_I:
        return gb_shifted ? 'I' : 'i';
    case KEY_O:
        return gb_shifted ? 'O' : 'o';
    case KEY_P:
        return gb_shifted ? 'P' : 'p';
    case KEY_LBRACKET:
        return gb_shifted ? '{' : '[';
    case KEY_RBRACKET:
        return gb_shifted ? '}' : ']';
    case KEY_BACKSLASH:
        return gb_shifted ? '|' : '\\';
    case KEY_A:
        return gb_shifted ? 'A' : 'a';
    case KEY_S:
        return gb_shifted ? 'S' : 's';
    case KEY_D:
        return gb_shifted ? 'D' : 'd';
    case KEY_F:
        return gb_shifted ? 'F' : 'f';
    case KEY_G:
        return gb_shifted ? 'G' : 'g';
    case KEY_H:
        return gb_shifted ? 'H' : 'h';
    case KEY_J:
        return gb_shifted ? 'J' : 'j';
    case KEY_K:
        return gb_shifted ? 'K' : 'k';
    case KEY_L:
        return gb_shifted ? 'L' : 'l';
    case KEY_SEMICOLON:
        return gb_shifted ? ':' : ';';
    case KEY_APOSTROPHE:
        return gb_shifted ? '"' : '\'';
    case KEY_ENTER:
        return '\n';
    case KEY_Z:
        return gb_shifted ? 'Z' : 'z';
    case KEY_X:
        return gb_shifted ? 'X' : 'x';
    case KEY_C:
        return gb_shifted ? 'C' : 'c';
    case KEY_V:
        return gb_shifted ? 'V' : 'v';
    case KEY_B:
        return gb_shifted ? 'B' : 'b';
    case KEY_N:
        return gb_shifted ? 'N' : 'n';
    case KEY_M:
        return gb_shifted ? 'M' : 'm';
    case KEY_COMMA:
        return gb_shifted ? '<' : ',';
    case KEY_PERIOD:
        return gb_shifted ? '>' : '.';
    case KEY_SLASH:
        return gb_shifted ? '?' : '/';
    case KEY_SPACE:
        return ' ';

    default:
        return 0;
    }
}
