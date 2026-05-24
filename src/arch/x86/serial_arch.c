#include "serial_arch.h"

#include "arch/x86/port.h"

uint8_t serial_arch_read_reg(serial_ctx_t *serial, uint16_t offset) {
    return port_inb(serial->port_base + offset);
}

void serial_arch_write_reg(serial_ctx_t *serial, uint16_t offset,
                           uint8_t data) {
    port_outb(serial->port_base + offset, data);
}
