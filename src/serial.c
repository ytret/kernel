#include <stdint.h>

#include "kinttypes.h"
#include "log.h"
#include "panic.h"
#include "port.h"
#include "serial.h"

#define BIT(x) (1 << (x))

#define SERIAL_COM_BASE            0x03F8
#define SERIAL_BAUDRATE_115200_DIV 1
// NOTE: the actual baudrate depends on the crystal. Setting the divisor to 1
// makes the chip use the crystal frequency.

#define SERIAL_REG_RBR 0 //!< Receiver Buffer (r/o).
#define SERIAL_REG_THR 0 //!< Transmitter Holding (w/o).
#define SERIAL_REG_IER 1 //!< Interrupt Enable (r/w).
#define SERIAL_REG_DLL 0 //!< Divisor Latch Low Byte (r/w).
#define SERIAL_REG_DLH 1 //!< Divisor Latch High Byte (r/w).
#define SERIAL_REG_IIR 2 //!< Interrupt Identification (r/o).
#define SERIAL_REG_FCR 2 //!< FIFO Control (w/o).
#define SERIAL_REG_LCR 3 //!< Line Control (r/w).
#define SERIAL_REG_MCR 4 //!< Modem Control (r/w).
#define SERIAL_REG_LSR 5 //!< Line Status (r/o).
#define SERIAL_REG_MSR 6 //!< Modem Status (r/o).
#define SERIAL_REG_SR  7 //!< Scratch (r/w).

#define SERIAL_FCR_FIFO_EN     BIT(0)
#define SERIAL_FCR_RX_FIFO_RST BIT(1)
#define SERIAL_FCR_TX_FIFO_RST BIT(2)
#define SERIAL_FCR_DMA_MODE_1  BIT(3)
#define SERIAL_FCR_RX_TRG_POS  6
#define SERIAL_FCR_RX_TRG_MSK  0xC0
#define SERIAL_FCR_RX_TRG_01   (0x00 << SERIAL_FCR_RX_TRG_POS)
#define SERIAL_FCR_RX_TRG_04   (0x01 << SERIAL_FCR_RX_TRG_POS)
#define SERIAL_FCR_RX_TRG_08   (0x02 << SERIAL_FCR_RX_TRG_POS)
#define SERIAL_FCR_RX_TRG_14   (0x03 << SERIAL_FCR_RX_TRG_POS)

#define SERIAL_LCR_DLAB         BIT(7) //!< Divisor Latch Access Bit.
#define SERIAL_LCR_SET_BREAK    BIT(6)
#define SERIAL_LCR_STICK_PARITY BIT(5)
#define SERIAL_LCR_EVEN_PARITY  BIT(4)
#define SERIAL_LCR_EN_PARITY    BIT(3)
#define SERIAL_LCR_STOP_BITS    BIT(2)
#define SERIAL_LCR_WLS_POS      0    //!< World Length Select offset.
#define SERIAL_LCR_WLS_MSK      0x03 //!< World Length Select mask.
#define SERIAL_LCR_WLS_5        0x00
#define SERIAL_LCR_WLS_6        0x01
#define SERIAL_LCR_WLS_7        0x02
#define SERIAL_LCR_WLS_8        0x03

#define SERIAL_MCR_DTR   BIT(0) //!< Set DTR (Data Terminal Ready).
#define SERIAL_MCR_RTS   BIT(1) //!< Set RTS (Request to Send).
#define SERIAL_MCR_OUT_1 BIT(2)
#define SERIAL_MCR_OUT_2 BIT(3)
#define SERIAL_MCR_LOOP  BIT(4)
#define SERIAL_MCR_AF    BIT(5) //!< Autoflow (may be reserved).

#define SERIAL_LSR_DR   BIT(0) //!< Data Ready.
#define SERIAL_LSR_OE   BIT(1) //!< Overrun Error.
#define SERIAL_LSR_PE   BIT(2) //!< Parity Error.
#define SERIAL_LSR_FE   BIT(3) //!< Framing Error.
#define SERIAL_LSR_BI   BIT(4) //!< Break Interrupt.
#define SERIAL_LSR_THRE BIT(5) //!< Transmitter Holding Register Empty.
#define SERIAL_LSR_TEMT BIT(6) //!< THRE, Shift Register, and TX FIFO are empty.
#define SERIAL_LSR_     BIT(6) //!< Transmitter Empty.

typedef struct {
    uint16_t base;
    uint16_t div;
} serial_ctx_t;

static serial_ctx_t g_serial;

static uint8_t prv_serial_read_reg(serial_ctx_t *serial, uint16_t offset);
static void prv_serial_write_reg(serial_ctx_t *serial, uint16_t offset,
                                 uint8_t data);

static void prv_serial_set_dlab(serial_ctx_t *serial, bool on);
static void prv_serial_set_div(serial_ctx_t *serial);
static void prv_serial_set_fcr(serial_ctx_t *serial);
static void prv_serial_set_lcr(serial_ctx_t *serial);
static void prv_serial_set_mcr(serial_ctx_t *serial);

static void prv_serial_transmit(serial_ctx_t *serial, uint8_t data);

void serial_init(void) {
    g_serial.base = SERIAL_COM_BASE;
    g_serial.div = SERIAL_BAUDRATE_115200_DIV;

    prv_serial_set_div(&g_serial);
    prv_serial_set_fcr(&g_serial);
    prv_serial_set_lcr(&g_serial);
    prv_serial_set_mcr(&g_serial);
}

void serial_puts(const char *str) {
    if (!str) { return; }
    if (g_serial.base == 0) { return; }
    while (*str) {
        // FIXME: check if the buffer is not full?
        prv_serial_transmit(&g_serial, *str);
        str++;
    }
}

static uint8_t prv_serial_read_reg(serial_ctx_t *serial, uint16_t offset) {
    if (g_serial.base == 0) {
        LOG_ERROR("tried to read register 0x%04" PRIx32
                  " in uninitialized state",
                  (uint32_t)offset);
        panic("unexpected behavior");
    }
    return port_inb(serial->base + offset);
}

static void prv_serial_write_reg(serial_ctx_t *serial, uint16_t offset,
                                 uint8_t data) {
    if (g_serial.base == 0) {
        LOG_ERROR("tried to write register 0x%04" PRIx32
                  " in uninitialized state",
                  (uint32_t)offset);
        panic("unexpected behavior");
    }
    port_outb(serial->base + offset, data);
}

static void prv_serial_set_dlab(serial_ctx_t *serial, bool on) {
    uint8_t reg_val = prv_serial_read_reg(serial, SERIAL_REG_LCR);
    reg_val = on ? (reg_val | SERIAL_LCR_DLAB) : (reg_val & ~SERIAL_LCR_DLAB);
    prv_serial_write_reg(serial, SERIAL_REG_LCR, reg_val);
}

static void prv_serial_set_div(serial_ctx_t *serial) {
    const uint16_t div = serial->div;

    prv_serial_set_dlab(serial, true);
    prv_serial_write_reg(serial, SERIAL_REG_DLH, (uint8_t)(div >> 8));
    prv_serial_write_reg(serial, SERIAL_REG_DLL, (uint8_t)div);
    prv_serial_set_dlab(serial, false);
}

static void prv_serial_set_fcr(serial_ctx_t *serial) {
    // Older chips do not support FCR, because they did not use FIFO buffers. On
    // newer chips bit 5 is used for larger buffer sizes, but here it's assumed
    // to be reserved.

    uint8_t reg_val = 0x00;

    // Writes with this bet clear are ignored.
    reg_val |= SERIAL_FCR_FIFO_EN;

    // Reset the buffers. These bits are self-clearing.
    reg_val |= SERIAL_FCR_RX_FIFO_RST;
    reg_val |= SERIAL_FCR_TX_FIFO_RST;

    reg_val &= ~SERIAL_FCR_DMA_MODE_1;

    // Generate the RX interrupt when there are 14 bytes in the RX buffer.
    reg_val |= SERIAL_FCR_RX_TRG_14;

    prv_serial_write_reg(serial, SERIAL_REG_FCR, reg_val);
}

static void prv_serial_set_lcr(serial_ctx_t *serial) {
    uint8_t reg_val = 0x00;

    reg_val &= ~SERIAL_LCR_WLS_MSK;
    reg_val |= (SERIAL_LCR_WLS_8 << SERIAL_LCR_WLS_MSK);

    reg_val &= ~SERIAL_LCR_STOP_BITS;

    reg_val &= ~SERIAL_LCR_EN_PARITY;
    reg_val &= ~SERIAL_LCR_STICK_PARITY;

    reg_val &= ~SERIAL_LCR_SET_BREAK;

    prv_serial_write_reg(serial, SERIAL_REG_LCR, reg_val);
}

static void prv_serial_set_mcr(serial_ctx_t *serial) {
    uint8_t reg_val = 0x00;

    // Not all chips support autoflow. Setting the bit on those is assumed to do
    // nothing.
    reg_val |= SERIAL_MCR_AF;

    prv_serial_write_reg(serial, SERIAL_REG_MCR, reg_val);
}

static void prv_serial_transmit(serial_ctx_t *serial, uint8_t data) {
    prv_serial_write_reg(serial, SERIAL_REG_THR, data);
}
