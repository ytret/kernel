/**
 * @file
 * Keymap layer.
 *
 * A "keymap" object is responsible for transforming keyboard press events into
 * byte sequences (e.g., ANSI escape sequences or simple ASCII characters).
 */

#pragma once

#include <stddef.h>

#include "kbd.h"

#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04

#define KEY_NUMLOCK_OFF_NONE (-1)

typedef struct keymap {
    bool lshift : 1;
    bool rshift : 1;
    bool lctrl : 1;
    bool rctrl : 1;
    bool lalt : 1;
    bool ralt : 1;
    bool caps_lock : 1;
    bool num_lock : 1;
    bool scroll_lock : 1;
} keymap_t;

void keymap_init(keymap_t *keymap);
size_t keymap_process(keymap_t *keymap, const kbd_event_t *event, void *buf,
                      size_t buf_size);
bool keymap_is_alt_pressed(keymap_t *keymap);
