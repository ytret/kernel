// NOTE: this file is partially AI-generated.

#include "keymap.h"

typedef struct {
    uint8_t mods;
    const char *seq;
} kbd_mod_map_t;

typedef struct {
    const kbd_mod_map_t *mods;
    int8_t numlock_off_key;
} keymap_seq_t;

typedef struct {
    bool lshift : 1;
    bool rshift : 1;
    bool lctrl  : 1;
    bool rctrl  : 1;
    bool lalt   : 1;
    bool ralt   : 1;
    bool caps_lock : 1;
    bool num_lock : 1;
    bool scroll_lock : 1;
} keymap_t;

static keymap_t g_keymap;

#define CHARR(noshift, shift, ctrl)                                            \
    ((const kbd_mod_map_t[]){                                                  \
        {0, noshift},                                                          \
        {MOD_SHIFT, shift},                                                    \
        {MOD_CTRL, ctrl},                                                      \
        {MOD_SHIFT | MOD_CTRL, ctrl},                                          \
        {MOD_ALT, "\033" noshift},                                             \
        {MOD_SHIFT | MOD_ALT, "\033" shift},                                   \
        {MOD_CTRL | MOD_ALT, "\033" ctrl},                                     \
        {0, NULL},                                                             \
    })

static const kbd_mod_map_t k_none[] = {
    {0, NULL},
};

// ── Modifier keys ───────────────────────────────────────────────────
// These are handled by keymap_process state tracking; no entries needed.
// The g_keymap_seqs slots for them remain zero-initialized (mods = NULL).

// ── Function keys ───────────────────────────────────────────────────
static const kbd_mod_map_t k_f1[] = {
    {0, "\x1bOP"},
    {MOD_SHIFT, "\x1b[1;2P"},
    {MOD_CTRL, "\x1b[1;5P"},
    {MOD_ALT, "\x1b[1;3P"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6P"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4P"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7P"},
    {0, NULL},
};
static const kbd_mod_map_t k_f2[] = {
    {0, "\x1bOQ"},
    {MOD_SHIFT, "\x1b[1;2Q"},
    {MOD_CTRL, "\x1b[1;5Q"},
    {MOD_ALT, "\x1b[1;3Q"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6Q"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4Q"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7Q"},
    {0, NULL},
};
static const kbd_mod_map_t k_f3[] = {
    {0, "\x1bOR"},
    {MOD_SHIFT, "\x1b[1;2R"},
    {MOD_CTRL, "\x1b[1;5R"},
    {MOD_ALT, "\x1b[1;3R"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6R"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4R"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7R"},
    {0, NULL},
};
static const kbd_mod_map_t k_f4[] = {
    {0, "\x1bOS"},
    {MOD_SHIFT, "\x1b[1;2S"},
    {MOD_CTRL, "\x1b[1;5S"},
    {MOD_ALT, "\x1b[1;3S"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6S"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4S"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7S"},
    {0, NULL},
};
static const kbd_mod_map_t k_f5[] = {
    {0, "\x1b[15~"},
    {MOD_SHIFT, "\x1b[15;2~"},
    {MOD_CTRL, "\x1b[15;5~"},
    {MOD_ALT, "\x1b[15;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[15;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[15;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[15;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f6[] = {
    {0, "\x1b[17~"},
    {MOD_SHIFT, "\x1b[17;2~"},
    {MOD_CTRL, "\x1b[17;5~"},
    {MOD_ALT, "\x1b[17;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[17;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[17;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[17;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f7[] = {
    {0, "\x1b[18~"},
    {MOD_SHIFT, "\x1b[18;2~"},
    {MOD_CTRL, "\x1b[18;5~"},
    {MOD_ALT, "\x1b[18;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[18;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[18;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[18;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f8[] = {
    {0, "\x1b[19~"},
    {MOD_SHIFT, "\x1b[19;2~"},
    {MOD_CTRL, "\x1b[19;5~"},
    {MOD_ALT, "\x1b[19;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[19;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[19;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[19;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f9[] = {
    {0, "\x1b[20~"},
    {MOD_SHIFT, "\x1b[20;2~"},
    {MOD_CTRL, "\x1b[20;5~"},
    {MOD_ALT, "\x1b[20;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[20;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[20;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[20;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f10[] = {
    {0, "\x1b[21~"},
    {MOD_SHIFT, "\x1b[21;2~"},
    {MOD_CTRL, "\x1b[21;5~"},
    {MOD_ALT, "\x1b[21;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[21;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[21;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[21;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f11[] = {
    {0, "\x1b[23~"},
    {MOD_SHIFT, "\x1b[23;2~"},
    {MOD_CTRL, "\x1b[23;5~"},
    {MOD_ALT, "\x1b[23;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[23;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[23;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[23;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_f12[] = {
    {0, "\x1b[24~"},
    {MOD_SHIFT, "\x1b[24;2~"},
    {MOD_CTRL, "\x1b[24;5~"},
    {MOD_ALT, "\x1b[24;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[24;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[24;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[24;7~"},
    {0, NULL},
};

// ── Escape ──────────────────────────────────────────────────────────
static const kbd_mod_map_t k_esc[] = {
    {0, "\x1b"},        {MOD_SHIFT, "\x1b"},
    {MOD_CTRL, "\x1b"}, {MOD_SHIFT | MOD_CTRL, "\x1b"},
    {0, NULL},
};

// ── Tab, Space, Enter, Backspace ────────────────────────────────────
static const kbd_mod_map_t k_tab[] = {
    {0, "\t"},
    {MOD_SHIFT, "\x1b[Z"},
    {0, NULL},
};
static const kbd_mod_map_t k_space[] = {
    {0, " "},  {MOD_SHIFT, " "}, {MOD_CTRL, ""}, {MOD_SHIFT | MOD_CTRL, ""},
    {0, NULL},
};
static const kbd_mod_map_t k_enter[] = {
    {0, "\n"},
    {MOD_SHIFT, "\n"},
    {0, NULL},
};
static const kbd_mod_map_t k_backspace[] = {
    {0, "\x7f"},
    {MOD_SHIFT, "\x7f"},
    {0, NULL},
};

// ── Alphabet ────────────────────────────────────────────────────────
// Ctrl+A..Z produces \x01..\x1a
static const kbd_mod_map_t k_a[] = CHARR("a", "A", "\x01");
static const kbd_mod_map_t k_b[] = CHARR("b", "B", "\x02");
static const kbd_mod_map_t k_c[] = CHARR("c", "C", "\x03");
static const kbd_mod_map_t k_d[] = CHARR("d", "D", "\x04");
static const kbd_mod_map_t k_e[] = CHARR("e", "E", "\x05");
static const kbd_mod_map_t k_f[] = CHARR("f", "F", "\x06");
static const kbd_mod_map_t k_g[] = CHARR("g", "G", "\x07");
static const kbd_mod_map_t k_h[] = CHARR("h", "H", "\x08");
static const kbd_mod_map_t k_i[] = CHARR("i", "I", "\x09");
static const kbd_mod_map_t k_j[] = CHARR("j", "J", "\x0a");
static const kbd_mod_map_t k_k[] = CHARR("k", "K", "\x0b");
static const kbd_mod_map_t k_l[] = CHARR("l", "L", "\x0c");
static const kbd_mod_map_t k_m[] = CHARR("m", "M", "\x0d");
static const kbd_mod_map_t k_n[] = CHARR("n", "N", "\x0e");
static const kbd_mod_map_t k_o[] = CHARR("o", "O", "\x0f");
static const kbd_mod_map_t k_p[] = CHARR("p", "P", "\x10");
static const kbd_mod_map_t k_q[] = CHARR("q", "Q", "\x11");
static const kbd_mod_map_t k_r[] = CHARR("r", "R", "\x12");
static const kbd_mod_map_t k_s[] = CHARR("s", "S", "\x13");
static const kbd_mod_map_t k_t[] = CHARR("t", "T", "\x14");
static const kbd_mod_map_t k_u[] = CHARR("u", "U", "\x15");
static const kbd_mod_map_t k_v[] = CHARR("v", "V", "\x16");
static const kbd_mod_map_t k_w[] = CHARR("w", "W", "\x17");
static const kbd_mod_map_t k_x[] = CHARR("x", "X", "\x18");
static const kbd_mod_map_t k_y[] = CHARR("y", "Y", "\x19");
static const kbd_mod_map_t k_z[] = CHARR("z", "Z", "\x1a");

// ── Digits ──────────────────────────────────────────────────────────
static const kbd_mod_map_t k_1[] = CHARR("1", "!", "1");
static const kbd_mod_map_t k_2[] = CHARR("2", "@", "2");
static const kbd_mod_map_t k_3[] = CHARR("3", "#", "3");
static const kbd_mod_map_t k_4[] = CHARR("4", "$", "4");
static const kbd_mod_map_t k_5[] = CHARR("5", "%", "5");
static const kbd_mod_map_t k_6[] = CHARR("6", "^", "\x1e");
static const kbd_mod_map_t k_7[] = CHARR("7", "&", "7");
static const kbd_mod_map_t k_8[] = CHARR("8", "*", "8");
static const kbd_mod_map_t k_9[] = CHARR("9", "(", "9");
static const kbd_mod_map_t k_0[] = CHARR("0", ")", "0");

// ── Symbols ─────────────────────────────────────────────────────────
static const kbd_mod_map_t k_bashtick[] = CHARR("`", "~", "`");
static const kbd_mod_map_t k_minus[] = CHARR("-", "_", "\x1f");
static const kbd_mod_map_t k_equals[] = CHARR("=", "+", "=");
static const kbd_mod_map_t k_lbracket[] = CHARR("[", "{", "\x1b");
static const kbd_mod_map_t k_rbracket[] = CHARR("]", "}", "\x1d");
static const kbd_mod_map_t k_backslash[] = CHARR("\\", "|", "\x1c");
static const kbd_mod_map_t k_semicolon[] = CHARR(";", ":", ";");
static const kbd_mod_map_t k_apostrophe[] = CHARR("'", "\"", "'");
static const kbd_mod_map_t k_comma[] = CHARR(",", "<", ",");
static const kbd_mod_map_t k_period[] = CHARR(".", ">", ".");
static const kbd_mod_map_t k_slash[] = CHARR("/", "?", "/");

// ── Cursor / editing keys (ANSI) ────────────────────────────────────
static const kbd_mod_map_t k_insert[] = {
    {0, "\x1b[2~"},
    {MOD_SHIFT, "\x1b[2;2~"},
    {MOD_CTRL, "\x1b[2;5~"},
    {MOD_ALT, "\x1b[2;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[2;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[2;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[2;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_delete[] = {
    {0, "\x1b[3~"},
    {MOD_SHIFT, "\x1b[3;2~"},
    {MOD_CTRL, "\x1b[3;5~"},
    {MOD_ALT, "\x1b[3;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[3;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[3;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[3;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_home[] = {
    {0, "\x1b[H"},
    {MOD_SHIFT, "\x1b[1;2H"},
    {MOD_CTRL, "\x1b[1;5H"},
    {MOD_ALT, "\x1b[1;3H"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6H"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4H"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7H"},
    {0, NULL},
};
static const kbd_mod_map_t k_end[] = {
    {0, "\x1b[F"},
    {MOD_SHIFT, "\x1b[1;2F"},
    {MOD_CTRL, "\x1b[1;5F"},
    {MOD_ALT, "\x1b[1;3F"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6F"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4F"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7F"},
    {0, NULL},
};
static const kbd_mod_map_t k_pageup[] = {
    {0, "\x1b[5~"},
    {MOD_SHIFT, "\x1b[5;2~"},
    {MOD_CTRL, "\x1b[5;5~"},
    {MOD_ALT, "\x1b[5;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[5;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[5;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[5;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_pagedown[] = {
    {0, "\x1b[6~"},
    {MOD_SHIFT, "\x1b[6;2~"},
    {MOD_CTRL, "\x1b[6;5~"},
    {MOD_ALT, "\x1b[6;3~"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[6;6~"},
    {MOD_SHIFT | MOD_ALT, "\x1b[6;4~"},
    {MOD_CTRL | MOD_ALT, "\x1b[6;7~"},
    {0, NULL},
};
static const kbd_mod_map_t k_left[] = {
    {0, "\x1b[D"},
    {MOD_SHIFT, "\x1b[1;2D"},
    {MOD_CTRL, "\x1b[1;5D"},
    {MOD_ALT, "\x1b[1;3D"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6D"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4D"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7D"},
    {0, NULL},
};
static const kbd_mod_map_t k_up[] = {
    {0, "\x1b[A"},
    {MOD_SHIFT, "\x1b[1;2A"},
    {MOD_CTRL, "\x1b[1;5A"},
    {MOD_ALT, "\x1b[1;3A"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6A"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4A"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7A"},
    {0, NULL},
};
static const kbd_mod_map_t k_down[] = {
    {0, "\x1b[B"},
    {MOD_SHIFT, "\x1b[1;2B"},
    {MOD_CTRL, "\x1b[1;5B"},
    {MOD_ALT, "\x1b[1;3B"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6B"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4B"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7B"},
    {0, NULL},
};
static const kbd_mod_map_t k_right[] = {
    {0, "\x1b[C"},
    {MOD_SHIFT, "\x1b[1;2C"},
    {MOD_CTRL, "\x1b[1;5C"},
    {MOD_ALT, "\x1b[1;3C"},
    {MOD_SHIFT | MOD_CTRL, "\x1b[1;6C"},
    {MOD_SHIFT | MOD_ALT, "\x1b[1;4C"},
    {MOD_CTRL | MOD_ALT, "\x1b[1;7C"},
    {0, NULL},
};

// ── Numpad keys (numlock-aware) ─────────────────────────────────────
// With numlock ON they act as digits/symbols.
// With numlock OFF they remap to navigation keys.
static const kbd_mod_map_t k_np_num[] = {
    {0, "7"},
    {MOD_SHIFT, "7"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_0[] = {
    {0, "0"},
    {MOD_SHIFT, "0"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_1[] = {
    {0, "1"},
    {MOD_SHIFT, "1"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_2[] = {
    {0, "2"},
    {MOD_SHIFT, "2"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_3[] = {
    {0, "3"},
    {MOD_SHIFT, "3"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_4[] = {
    {0, "4"},
    {MOD_SHIFT, "4"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_5[] = {
    {0, "5"},
    {MOD_SHIFT, "5"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_6[] = {
    {0, "6"},
    {MOD_SHIFT, "6"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_8[] = {
    {0, "8"},
    {MOD_SHIFT, "8"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_num_9[] = {
    {0, "9"},
    {MOD_SHIFT, "9"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_dot[] = {
    {0, "."},
    {MOD_SHIFT, "."},
    {0, NULL},
};
static const kbd_mod_map_t k_np_enter[] = {
    {0, "\n"},
    {MOD_SHIFT, "\n"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_slash[] = {
    {0, "/"},
    {MOD_SHIFT, "/"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_ast[] = {
    {0, "*"},
    {MOD_SHIFT, "*"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_minus[] = {
    {0, "-"},
    {MOD_SHIFT, "-"},
    {0, NULL},
};
static const kbd_mod_map_t k_np_plus[] = {
    {0, "+"},
    {MOD_SHIFT, "+"},
    {0, NULL},
};

// ── Static keys (no output) ─────────────────────────────────────────
static const kbd_mod_map_t k_prtsc[] = {
    {0, NULL},
};
static const kbd_mod_map_t k_pause[] = {
    {0, NULL},
};

// ── Main keymap table ───────────────────────────────────────────────
// Keys not listed here (such as KEY_LSHIFT, KEY_LCTRL, etc.) are
// zero-initialized (mods = NULL) and handled as modifier keys.
static const keymap_seq_t g_keymap_seqs[] = {
    [KEY_ESCAPE] = {k_esc, -1},
    [KEY_BACKTICK] = {k_bashtick, -1},
    [KEY_TAB] = {k_tab, -1},
    [KEY_CAPSLOCK] = {NULL, -1},
    [KEY_LSHIFT] = {NULL, -1},
    [KEY_RSHIFT] = {NULL, -1},
    [KEY_LCTRL] = {NULL, -1},
    [KEY_LALT] = {NULL, -1},
    [KEY_SPACE] = {k_space, -1},
    [KEY_F1] = {k_f1, -1},
    [KEY_F2] = {k_f2, -1},
    [KEY_F3] = {k_f3, -1},
    [KEY_F4] = {k_f4, -1},
    [KEY_F5] = {k_f5, -1},
    [KEY_F6] = {k_f6, -1},
    [KEY_F7] = {k_f7, -1},
    [KEY_F8] = {k_f8, -1},
    [KEY_F9] = {k_f9, -1},
    [KEY_F10] = {k_f10, -1},
    [KEY_F11] = {k_f11, -1},
    [KEY_F12] = {k_f12, -1},
    [KEY_NUMLOCK] = {NULL, -1},
    [KEY_SCROLLLOCK] = {NULL, -1},
    [KEY_1] = {k_1, -1},
    [KEY_2] = {k_2, -1},
    [KEY_3] = {k_3, -1},
    [KEY_4] = {k_4, -1},
    [KEY_5] = {k_5, -1},
    [KEY_6] = {k_6, -1},
    [KEY_7] = {k_7, -1},
    [KEY_8] = {k_8, -1},
    [KEY_9] = {k_9, -1},
    [KEY_0] = {k_0, -1},
    [KEY_MINUS] = {k_minus, -1},
    [KEY_EQUALS] = {k_equals, -1},
    [KEY_BACKSPACE] = {k_backspace, -1},
    [KEY_Q] = {k_q, -1},
    [KEY_W] = {k_w, -1},
    [KEY_E] = {k_e, -1},
    [KEY_R] = {k_r, -1},
    [KEY_T] = {k_t, -1},
    [KEY_Y] = {k_y, -1},
    [KEY_U] = {k_u, -1},
    [KEY_I] = {k_i, -1},
    [KEY_O] = {k_o, -1},
    [KEY_P] = {k_p, -1},
    [KEY_LBRACKET] = {k_lbracket, -1},
    [KEY_RBRACKET] = {k_rbracket, -1},
    [KEY_BACKSLASH] = {k_backslash, -1},
    [KEY_A] = {k_a, -1},
    [KEY_S] = {k_s, -1},
    [KEY_D] = {k_d, -1},
    [KEY_F] = {k_f, -1},
    [KEY_G] = {k_g, -1},
    [KEY_H] = {k_h, -1},
    [KEY_J] = {k_j, -1},
    [KEY_K] = {k_k, -1},
    [KEY_L] = {k_l, -1},
    [KEY_SEMICOLON] = {k_semicolon, -1},
    [KEY_APOSTROPHE] = {k_apostrophe, -1},
    [KEY_ENTER] = {k_enter, -1},
    [KEY_Z] = {k_z, -1},
    [KEY_X] = {k_x, -1},
    [KEY_C] = {k_c, -1},
    [KEY_V] = {k_v, -1},
    [KEY_B] = {k_b, -1},
    [KEY_N] = {k_n, -1},
    [KEY_M] = {k_m, -1},
    [KEY_COMMA] = {k_comma, -1},
    [KEY_PERIOD] = {k_period, -1},
    [KEY_SLASH] = {k_slash, -1},
    [KEY_NPASTERISK] = {k_np_ast, KEY_NUMLOCK_OFF_NONE},
    [KEY_NPMINUS] = {k_np_minus, KEY_NUMLOCK_OFF_NONE},
    [KEY_NPPLUS] = {k_np_plus, KEY_NUMLOCK_OFF_NONE},
    [KEY_NPPERIOD] = {k_np_dot, KEY_DELETE},
    [KEY_NP1] = {k_np_num_1, KEY_END},
    [KEY_NP2] = {k_np_num_2, KEY_DOWNARROW},
    [KEY_NP3] = {k_np_num_3, KEY_PAGEDOWN},
    [KEY_NP4] = {k_np_num_4, KEY_LEFTARROW},
    [KEY_NP5] = {k_np_num_5, KEY_NUMLOCK_OFF_NONE},
    [KEY_NP6] = {k_np_num_6, KEY_RIGHTARROW},
    [KEY_NP7] = {k_np_num, KEY_HOME},
    [KEY_NP8] = {k_np_num_8, KEY_UPARROW},
    [KEY_NP9] = {k_np_num_9, KEY_PAGEUP},
    [KEY_NP0] = {k_np_num_0, KEY_INSERT},
    [KEY_RCTRL] = {NULL, -1},
    [KEY_RALT] = {NULL, -1},
    [KEY_MENU] = {k_none, -1},
    [KEY_SUPER] = {k_none, -1},
    [KEY_INSERT] = {k_insert, -1},
    [KEY_DELETE] = {k_delete, -1},
    [KEY_HOME] = {k_home, -1},
    [KEY_END] = {k_end, -1},
    [KEY_PAGEUP] = {k_pageup, -1},
    [KEY_PAGEDOWN] = {k_pagedown, -1},
    [KEY_LEFTARROW] = {k_left, -1},
    [KEY_UPARROW] = {k_up, -1},
    [KEY_DOWNARROW] = {k_down, -1},
    [KEY_RIGHTARROW] = {k_right, -1},
    [KEY_NPSLASH] = {k_np_slash, KEY_NUMLOCK_OFF_NONE},
    [KEY_NPENTER] = {k_np_enter, KEY_ENTER},
    [KEY_PRINTSCREEN] = {k_prtsc, -1},
    [KEY_PAUSEBREAK] = {k_pause, -1},
};

static size_t prv_keymap_write(const char *seq, char *buf, size_t buf_size);

void keymap_init(void) {
    g_keymap = (keymap_t){0};
}

size_t keymap_process(const kbd_event_t *event, void *buf, size_t buf_size) {
    uint8_t key = event->key;

    // ── Update modifier state (both press and release) ──────────────
    switch (key) {
    case KEY_LSHIFT:
        g_keymap.lshift = !event->b_released;
        return 0;
    case KEY_RSHIFT:
        g_keymap.rshift = !event->b_released;
        return 0;
    case KEY_LCTRL:
        g_keymap.lctrl = !event->b_released;
        return 0;
    case KEY_RCTRL:
        g_keymap.rctrl = !event->b_released;
        return 0;
    case KEY_LALT:
        g_keymap.lalt = !event->b_released;
        return 0;
    case KEY_RALT:
        g_keymap.ralt = !event->b_released;
        return 0;
    case KEY_CAPSLOCK:
        if (!event->b_released) { g_keymap.caps_lock = !g_keymap.caps_lock; }
        return 0;
    case KEY_NUMLOCK:
        if (!event->b_released) { g_keymap.num_lock = !g_keymap.num_lock; }
        return 0;
    case KEY_SCROLLLOCK:
        if (!event->b_released) {
            g_keymap.scroll_lock = !g_keymap.scroll_lock;
        }
        return 0;
    }

    // ── Release events produce no output ────────────────────────────
    if (event->b_released) { return 0; }

    // ── Look up key in map ──────────────────────────────────────────
    if (key >= sizeof(g_keymap_seqs) / sizeof(g_keymap_seqs[0])) { return 0; }
    const keymap_seq_t *ks = &g_keymap_seqs[key];

    // ── Numpad numlock-off remapping ────────────────────────────────
    uint8_t lookup_key = key;
    if (ks->numlock_off_key >= 0 && !g_keymap.num_lock) {
        lookup_key = (uint8_t)ks->numlock_off_key;
        if (lookup_key >= sizeof(g_keymap_seqs) / sizeof(g_keymap_seqs[0])) {
            return 0;
        }
        ks = &g_keymap_seqs[lookup_key];
    }

    // ── Build effective modifier mask ───────────────────────────────
    uint8_t mods = (g_keymap.lshift || g_keymap.rshift ? MOD_SHIFT : 0) |
                   (g_keymap.lctrl  || g_keymap.rctrl  ? MOD_CTRL  : 0) |
                   (g_keymap.lalt   || g_keymap.ralt   ? MOD_ALT   : 0);

    // Caps lock inverts Shift for letter keys
    if (g_keymap.caps_lock && ks->mods != NULL) {
        for (const kbd_mod_map_t *m = ks->mods; m->seq; m++) {
            if (m->mods == 0) {
                const char *s = m->seq;
                if (s[0] >= 'a' && s[0] <= 'z' && s[1] == '\0') {
                    mods ^= MOD_SHIFT;
                }
                break;
            }
        }
    }

    // ── Find matching entry ─────────────────────────────────────────
    if (ks->mods == NULL) { return 0; }

    const char *seq = NULL;
    for (const kbd_mod_map_t *m = ks->mods; m->seq; m++) {
        if (m->mods == mods) {
            seq = m->seq;
            break;
        }
    }
    if (seq == NULL) {
        // Fall back to first entry (no-modifier default)
        seq = ks->mods[0].seq;
    }
    if (seq == NULL) { return 0; }

    return prv_keymap_write(seq, buf, buf_size);
}

static size_t prv_keymap_write(const char *seq, char *buf, size_t buf_size) {
    char ch;
    size_t pos = 0;
    while ((ch = *seq++)) {
        if (pos >= buf_size) { break; }
        ((char *)buf)[pos] = ch;
        pos++;
    }
    return pos;
}
