#include <kbd.h>
#include <kshell.h>
#include <printf.h>

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
    // Parse only 'key pressed' events.
    //
    if (b_released)
    {
        return;
    }

    echo_key(key);
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
    if ((KEY_1 <= key) && (key <= KEY_9))
    {
        return ('1' + (key - KEY_1));
    }
    if (KEY_0 == key)
    {
        return ('0');
    }

    switch (key)
    {
        case KEY_MINUS: return ('-');
        case KEY_EQUALS: return ('=');
        case KEY_Q: return ('q');
        case KEY_W: return ('w');
        case KEY_E: return ('e');
        case KEY_R: return ('r');
        case KEY_T: return ('t');
        case KEY_Y: return ('y');
        case KEY_U: return ('u');
        case KEY_I: return ('i');
        case KEY_O: return ('o');
        case KEY_P: return ('p');
        case KEY_LBRACKET: return ('[');
        case KEY_RBRACKET: return (']');
        case KEY_BACKSLASH: return ('\\');
        case KEY_A: return ('a');
        case KEY_S: return ('s');
        case KEY_D: return ('d');
        case KEY_F: return ('f');
        case KEY_G: return ('g');
        case KEY_H: return ('h');
        case KEY_J: return ('j');
        case KEY_K: return ('k');
        case KEY_L: return ('l');
        case KEY_SEMICOLON: return (';');
        case KEY_APOSTROPHE: return ('\'');
        case KEY_ENTER: return ('\n');
        case KEY_Z: return ('z');
        case KEY_X: return ('x');
        case KEY_C: return ('c');
        case KEY_V: return ('v');
        case KEY_B: return ('b');
        case KEY_N: return ('n');
        case KEY_M: return ('m');
        case KEY_COMMA: return (',');
        case KEY_PERIOD: return ('.');
        case KEY_SLASH: return ('/');

        default:
            return (0);
    }
}
