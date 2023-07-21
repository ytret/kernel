#include <kbd.h>
#include <kshell.h>
#include <printf.h>
#include <term.h>

static volatile bool gb_shifted;

static void kbd_callback(uint8_t key, bool b_released);
static void echo_key(uint8_t key);
static char char_from_key(uint8_t key);

void
kshell_init (void)
{
    kbd_set_callback(kbd_callback);

    for (;;)
    {}
}

static void
kbd_callback (uint8_t key, bool b_released)
{
    if ((KEY_LSHIFT == key) || (KEY_RSHIFT == key))
    {
        gb_shifted = (!b_released);
    }

    // Henceforth parse only 'key pressed' events.
    //
    if (b_released)
    {
        return;
    }

    if (KEY_BACKSPACE == key)
    {
        size_t row = term_row();
        size_t col = term_col();

        if ((row == 0) && (col == 0))
        {
            return;
        }

        if (col > 0)
        {
            col--;
        }
        else if (row > 0)
        {
            row--;
            col = (term_width() - 1);
        }

        term_put_char_at(row, col, ' ');
        term_put_cursor_at(row, col);
    }
    else
    {
        echo_key(key);
    }
}

static void
echo_key (uint8_t key)
{
    char p_echoed[2] = { 0 };
    p_echoed[0] = char_from_key(key);
    if (p_echoed[0] != 0)
    {
        printf("%s", p_echoed);
    }
}

static char
char_from_key (uint8_t key)
{
    switch (key)
    {
        case KEY_BACKTICK:   return (gb_shifted ? '~' : '`');
        case KEY_1:          return (gb_shifted ? '!' : '1');
        case KEY_2:          return (gb_shifted ? '@' : '2');
        case KEY_3:          return (gb_shifted ? '#' : '3');
        case KEY_4:          return (gb_shifted ? '$' : '4');
        case KEY_5:          return (gb_shifted ? '%' : '5');
        case KEY_6:          return (gb_shifted ? '^' : '6');
        case KEY_7:          return (gb_shifted ? '&' : '7');
        case KEY_8:          return (gb_shifted ? '*' : '8');
        case KEY_9:          return (gb_shifted ? '(' : '9');
        case KEY_0:          return (gb_shifted ? ')' : '0');
        case KEY_MINUS:      return (gb_shifted ? '_' : '-');
        case KEY_EQUALS:     return (gb_shifted ? '+' : '=');
        case KEY_Q:          return (gb_shifted ? 'Q' : 'q');
        case KEY_W:          return (gb_shifted ? 'W' : 'w');
        case KEY_E:          return (gb_shifted ? 'E' : 'e');
        case KEY_R:          return (gb_shifted ? 'R' : 'r');
        case KEY_T:          return (gb_shifted ? 'T' : 't');
        case KEY_Y:          return (gb_shifted ? 'Y' : 'y');
        case KEY_U:          return (gb_shifted ? 'U' : 'u');
        case KEY_I:          return (gb_shifted ? 'I' : 'i');
        case KEY_O:          return (gb_shifted ? 'O' : 'o');
        case KEY_P:          return (gb_shifted ? 'P' : 'p');
        case KEY_LBRACKET:   return (gb_shifted ? '{' : '[');
        case KEY_RBRACKET:   return (gb_shifted ? '}' : ']');
        case KEY_BACKSLASH:  return (gb_shifted ? '|' : '\\');
        case KEY_A:          return (gb_shifted ? 'A' : 'a');
        case KEY_S:          return (gb_shifted ? 'S' : 's');
        case KEY_D:          return (gb_shifted ? 'D' : 'd');
        case KEY_F:          return (gb_shifted ? 'F' : 'f');
        case KEY_G:          return (gb_shifted ? 'G' : 'g');
        case KEY_H:          return (gb_shifted ? 'H' : 'h');
        case KEY_J:          return (gb_shifted ? 'J' : 'j');
        case KEY_K:          return (gb_shifted ? 'K' : 'k');
        case KEY_L:          return (gb_shifted ? 'L' : 'l');
        case KEY_SEMICOLON:  return (gb_shifted ? ':' : ';');
        case KEY_APOSTROPHE: return (gb_shifted ? '"' : '\'');
        case KEY_ENTER:      return ('\n');
        case KEY_Z:          return (gb_shifted ? 'Z' : 'z');
        case KEY_X:          return (gb_shifted ? 'X' : 'x');
        case KEY_C:          return (gb_shifted ? 'C' : 'c');
        case KEY_V:          return (gb_shifted ? 'V' : 'v');
        case KEY_B:          return (gb_shifted ? 'B' : 'b');
        case KEY_N:          return (gb_shifted ? 'N' : 'n');
        case KEY_M:          return (gb_shifted ? 'M' : 'm');
        case KEY_COMMA:      return (gb_shifted ? '<' : ',');
        case KEY_PERIOD:     return (gb_shifted ? '>' : '.');
        case KEY_SLASH:      return (gb_shifted ? '?' : '/');
        case KEY_SPACE:      return (' ');

        default:
            return (0);
    }
}
