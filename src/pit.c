#include <panic.h>
#include <pic.h>
#include <pit.h>
#include <port.h>
#include <printf.h>
#include <taskmgr.h>

#define PORT_CMD      0x0043
#define PORT_CH0_DATA 0x0040

#define REG_CH0         (0 << 6)
#define REG_BOTH_BYTES  (3 << 4)
#define REG_SQUARE_WAVE (3 << 1)

#define BASE_FREQ_KHZ 1193

static volatile bool gb_initialized;

void pit_init(uint8_t period_ms) {
    // Calculate the reload value.
    uint32_t reload_u32 = (BASE_FREQ_KHZ * period_ms);
    if (reload_u32 > 65535) {
        printf("PIT: reload value (%u) for period_ms = %u is too big\n",
               reload_u32, period_ms);
        printf("It must be less than or equal to 65535\n");
        panic("pit_init() failed");
    }

    // Send the mode/command register.
    uint8_t reg_byte = (REG_SQUARE_WAVE | REG_BOTH_BYTES | REG_CH0);
    port_outb(PORT_CMD, reg_byte);

    // Send the reload value.
    port_outb(PORT_CH0_DATA, ((uint8_t)reload_u32));
    port_outb(PORT_CH0_DATA, ((uint8_t)(reload_u32 >> 8)));

    gb_initialized = true;
}

void pit_irq_handler(void) {
    if (!gb_initialized) {
        printf("PIT: IRQ0 handler was called before initialization\n");
        panic("unexpected behavior");
    }

    pic_send_eoi(0);

    taskmgr_schedule();
}
