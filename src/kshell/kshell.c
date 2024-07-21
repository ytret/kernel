#include "kbd.h"
#include "kshell/cmd.h"
#include "kshell/kshell.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "term.h"

#define CMD_BUF_SIZE 256

// Shell state.
static volatile bool gb_reading_cmd;

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
static void kbd_callback(uint8_t key, bool b_released);
static void echo_key(uint8_t key);
static char char_from_key(uint8_t key);

void kshell(void) {
    for (;;) {
        // Set the keyboard event callback on every iteration, so that commands
        // can reset it to their own callbacks without affecting the prompt.
        kbd_set_callback(kbd_callback);

        printf("> ");
        char const *p_cmd = read_cmd();
        if (!p_cmd) { continue; }

        kshell_cmd_parse(p_cmd);
    }
}

/*
 * Synchronously reads a command from the user and returns it.
 *
 * NOTE: each call invalidates the pointer returned by the previous call.
 */
static char const *read_cmd(void) {
    gb_reading_cmd = true;

    // kbd_callback() resets gb_reading_cmd once KEY_ENTER is pressed.
    while (gb_reading_cmd) {}

    return ((char const *)buf_get_cmd());
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
        printf("kshell: command buffer overflow\n");
        panic("buffer overflow");
    }

    gp_cmd_buf[g_cmd_buf_pos] = 0;
    g_cmd_buf_pos = 0;
    return (gp_cmd_buf);
}

static void kbd_callback(uint8_t key, bool b_released) {
    if ((KEY_LSHIFT == key) || (KEY_RSHIFT == key)) {
        gb_shifted = (!b_released);
    }

    if (!gb_reading_cmd) { return; }

    // Henceforth parse only 'key pressed' events.
    if (b_released) { return; }

    if (KEY_BACKSPACE == key) {
        // Allow to delete as many characters, as there are in the command
        // buffer.
        if (0 == g_cmd_buf_pos) { return; }

        size_t row = term_row();
        size_t col = term_col();

        if ((row == 0) && (col == 0)) { return; }

        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = (term_width() - 1);
        }

        term_put_char_at(row, col, ' ');
        term_put_cursor_at(row, col);

        // Remove one char from the buffer.
        buf_remove();
    } else if (KEY_ENTER == key) {
        echo_key(key);
        gb_reading_cmd = false;
    } else {
        // Print the key and add it to the buffer, if it's a char.
        echo_key(key);

        char ch = char_from_key(key);
        if (ch) { buf_append(ch); }
    }
}

static void echo_key(uint8_t key) {
    char p_echoed[2] = {0};
    p_echoed[0] = char_from_key(key);
    if (p_echoed[0]) { printf("%s", p_echoed); }
}

static char char_from_key(uint8_t key) {
    switch (key) {
    case KEY_BACKTICK:
        return (gb_shifted ? '~' : '`');
    case KEY_1:
        return (gb_shifted ? '!' : '1');
    case KEY_2:
        return (gb_shifted ? '@' : '2');
    case KEY_3:
        return (gb_shifted ? '#' : '3');
    case KEY_4:
        return (gb_shifted ? '$' : '4');
    case KEY_5:
        return (gb_shifted ? '%' : '5');
    case KEY_6:
        return (gb_shifted ? '^' : '6');
    case KEY_7:
        return (gb_shifted ? '&' : '7');
    case KEY_8:
        return (gb_shifted ? '*' : '8');
    case KEY_9:
        return (gb_shifted ? '(' : '9');
    case KEY_0:
        return (gb_shifted ? ')' : '0');
    case KEY_MINUS:
        return (gb_shifted ? '_' : '-');
    case KEY_EQUALS:
        return (gb_shifted ? '+' : '=');
    case KEY_Q:
        return (gb_shifted ? 'Q' : 'q');
    case KEY_W:
        return (gb_shifted ? 'W' : 'w');
    case KEY_E:
        return (gb_shifted ? 'E' : 'e');
    case KEY_R:
        return (gb_shifted ? 'R' : 'r');
    case KEY_T:
        return (gb_shifted ? 'T' : 't');
    case KEY_Y:
        return (gb_shifted ? 'Y' : 'y');
    case KEY_U:
        return (gb_shifted ? 'U' : 'u');
    case KEY_I:
        return (gb_shifted ? 'I' : 'i');
    case KEY_O:
        return (gb_shifted ? 'O' : 'o');
    case KEY_P:
        return (gb_shifted ? 'P' : 'p');
    case KEY_LBRACKET:
        return (gb_shifted ? '{' : '[');
    case KEY_RBRACKET:
        return (gb_shifted ? '}' : ']');
    case KEY_BACKSLASH:
        return (gb_shifted ? '|' : '\\');
    case KEY_A:
        return (gb_shifted ? 'A' : 'a');
    case KEY_S:
        return (gb_shifted ? 'S' : 's');
    case KEY_D:
        return (gb_shifted ? 'D' : 'd');
    case KEY_F:
        return (gb_shifted ? 'F' : 'f');
    case KEY_G:
        return (gb_shifted ? 'G' : 'g');
    case KEY_H:
        return (gb_shifted ? 'H' : 'h');
    case KEY_J:
        return (gb_shifted ? 'J' : 'j');
    case KEY_K:
        return (gb_shifted ? 'K' : 'k');
    case KEY_L:
        return (gb_shifted ? 'L' : 'l');
    case KEY_SEMICOLON:
        return (gb_shifted ? ':' : ';');
    case KEY_APOSTROPHE:
        return (gb_shifted ? '"' : '\'');
    case KEY_ENTER:
        return ('\n');
    case KEY_Z:
        return (gb_shifted ? 'Z' : 'z');
    case KEY_X:
        return (gb_shifted ? 'X' : 'x');
    case KEY_C:
        return (gb_shifted ? 'C' : 'c');
    case KEY_V:
        return (gb_shifted ? 'V' : 'v');
    case KEY_B:
        return (gb_shifted ? 'B' : 'b');
    case KEY_N:
        return (gb_shifted ? 'N' : 'n');
    case KEY_M:
        return (gb_shifted ? 'M' : 'm');
    case KEY_COMMA:
        return (gb_shifted ? '<' : ',');
    case KEY_PERIOD:
        return (gb_shifted ? '>' : '.');
    case KEY_SLASH:
        return (gb_shifted ? '?' : '/');
    case KEY_SPACE:
        return (' ');

    default:
        return (0);
    }
}
