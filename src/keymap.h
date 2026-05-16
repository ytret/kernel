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

#define MOD_SHIFT  0x01
#define MOD_CTRL   0x02
#define MOD_ALT    0x04

#define KEY_NUMLOCK_OFF_NONE (-1)

void keymap_init(void);
size_t keymap_process(const kbd_event_t *event, void *buf, size_t buf_size);
