#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "queue.h"

#define KBD_IRQ 1

#define KEY_ESCAPE      0x00
#define KEY_BACKTICK    0x01
#define KEY_TAB         0x02
#define KEY_CAPSLOCK    0x03
#define KEY_LSHIFT      0x04
#define KEY_RSHIFT      0x05
#define KEY_LCTRL       0x06
#define KEY_LALT        0x07
#define KEY_SPACE       0x08
#define KEY_F1          0x09
#define KEY_F2          0x0a
#define KEY_F3          0x0b
#define KEY_F4          0x0c
#define KEY_F5          0x0d
#define KEY_F6          0x0e
#define KEY_F7          0x0f
#define KEY_F8          0x10
#define KEY_F9          0x11
#define KEY_F10         0x12
#define KEY_F11         0x13
#define KEY_F12         0x14
#define KEY_NUMLOCK     0x15
#define KEY_SCROLLLOCK  0x16
#define KEY_1           0x17
#define KEY_2           0x18
#define KEY_3           0x19
#define KEY_4           0x1a
#define KEY_5           0x1b
#define KEY_6           0x1c
#define KEY_7           0x1d
#define KEY_8           0x1e
#define KEY_9           0x1f
#define KEY_0           0x20
#define KEY_MINUS       0x21
#define KEY_EQUALS      0x22
#define KEY_BACKSPACE   0x23
#define KEY_Q           0x24
#define KEY_W           0x25
#define KEY_E           0x26
#define KEY_R           0x27
#define KEY_T           0x28
#define KEY_Y           0x29
#define KEY_U           0x2a
#define KEY_I           0x2b
#define KEY_O           0x2c
#define KEY_P           0x2d
#define KEY_LBRACKET    0x2e
#define KEY_RBRACKET    0x2f
#define KEY_BACKSLASH   0x30
#define KEY_A           0x31
#define KEY_S           0x32
#define KEY_D           0x33
#define KEY_F           0x34
#define KEY_G           0x35
#define KEY_H           0x36
#define KEY_J           0x37
#define KEY_K           0x38
#define KEY_L           0x39
#define KEY_SEMICOLON   0x3a
#define KEY_APOSTROPHE  0x3b
#define KEY_ENTER       0x3c
#define KEY_Z           0x3d
#define KEY_X           0x3e
#define KEY_C           0x3f
#define KEY_V           0x40
#define KEY_B           0x41
#define KEY_N           0x42
#define KEY_M           0x43
#define KEY_COMMA       0x44
#define KEY_PERIOD      0x45
#define KEY_SLASH       0x46
#define KEY_NPASTERISK  0x47
#define KEY_NPMINUS     0x48
#define KEY_NPPLUS      0x49
#define KEY_NPPERIOD    0x4a
#define KEY_NP1         0x4b
#define KEY_NP2         0x4c
#define KEY_NP3         0x4d
#define KEY_NP4         0x4e
#define KEY_NP5         0x4f
#define KEY_NP6         0x50
#define KEY_NP7         0x51
#define KEY_NP8         0x52
#define KEY_NP9         0x53
#define KEY_NP0         0x54
#define KEY_RCTRL       0x55
#define KEY_RALT        0x56
#define KEY_MENU        0x57
#define KEY_SUPER       0x58
#define KEY_INSERT      0x59
#define KEY_DELETE      0x5a
#define KEY_HOME        0x5b
#define KEY_END         0x5c
#define KEY_PAGEUP      0x5d
#define KEY_PAGEDOWN    0x5e
#define KEY_LEFTARROW   0x5f
#define KEY_UPARROW     0x60
#define KEY_DOWNARROW   0x61
#define KEY_RIGHTARROW  0x62
#define KEY_NPSLASH     0x63
#define KEY_NPENTER     0x64
#define KEY_PRINTSCREEN 0x65
#define KEY_PAUSEBREAK  0x66

typedef struct {
    queue_node_t queue_node;

    uint8_t key;
    bool b_released;
} kbd_event_t;

void kbd_init(void);
void kbd_irq_handler(void);
queue_t *kbd_event_queue(void);
queue_t *kbd_sysevent_queue(void);
