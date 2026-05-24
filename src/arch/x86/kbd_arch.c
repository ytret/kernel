#include "kbd_arch.h"

#include "arch/x86/port.h"

#define KBD_PORT_DATA   0x0060
#define KBD_PORT_CMD    0x0064
#define KBD_PORT_STATUS 0x0064

uint8_t kbd_arch_read_code() {
    return port_inb(KBD_PORT_DATA);
}
