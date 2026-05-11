#pragma once

#include <stdint.h>

#include "chardev.h"
#include "kspinlock.h"

#define SERIAL_COM1_BASE 0x03F8
#define SERIAL_COM2_BASE 0x02F8

/**
 * Divisor value for baudrate 115200.
 *
 * @note
 * The actual baudrate depends on the crystal. Setting the divisor to 1 makes
 * the chip use the crystal frequency.
 */
#define SERIAL_BAUDRATE_115200_DIV 1

typedef struct {
    uint16_t port_base;
    uint16_t baudrate_div;

    spinlock_t lock;
    bool ready;
} serial_ctx_t;

/**
 * Initializes a serial device @a serial.
 *
 * The fields #serial_ctx_t.port_base and #serial_ctx_t.baudrate_div must be
 * initialized.
 *
 * @param serial Serial device context.
 */
void serial_init(serial_ctx_t *serial);

/**
 * Initializes @a serial as a @a chardev.
 *
 * @param serial  Already initialized serial device context.
 * @param chardev Character device context to use.
 */
void serial_set_chardev(serial_ctx_t *serial, chardev_t *chardev);

int serial_write(void *v_serial, const void *buf, size_t buf_size);
