#pragma once

#include <stdbool.h>
#include <stdint.h>

void kshell(void);

/*
 * Sets the keyboard handler. Intended to be used only by command handlers.
 */
void kshell_set_kbd_handler(void (*p_handler)(uint8_t key, bool b_released));
