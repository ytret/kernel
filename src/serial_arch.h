#pragma once

#include <stdint.h>

#include "serial.h"

uint8_t serial_arch_read_reg(serial_ctx_t *serial, uint16_t offset);
void serial_arch_write_reg(serial_ctx_t *serial, uint16_t offset, uint8_t data);
