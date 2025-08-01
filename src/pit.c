#include "acpi/lapic.h"
#include "kprintf.h"
#include "panic.h"
#include "pit.h"
#include "port.h"

#define PORT_CMD      0x0043
#define PORT_CH0_DATA 0x0040

#define REG_CH0        (0 << 6)
#define REG_BOTH_BYTES (3 << 4)
#define REG_RATE_GEN   (2 << 1)

#define BASE_FREQ_KHZ 1193

static volatile bool gb_initialized;
static volatile _Atomic uint64_t g_counter_ms;

void pit_init(uint8_t period_ms) {
    // Calculate the reload value.
    uint32_t reload_u32 = BASE_FREQ_KHZ * period_ms;
    if (reload_u32 > 65535) {
        panic_enter();
        kprintf("pit: reload value (%u) for period_ms = %u is too big\n",
                reload_u32, period_ms);
        kprintf("pit: it must be less than or equal to 65535\n");
        panic("pit_init() failed");
    }

    // Send the mode/command register.
    uint8_t reg_byte = REG_RATE_GEN | REG_BOTH_BYTES | REG_CH0;
    port_outb(PORT_CMD, reg_byte);

    // Send the reload value.
    port_outb(PORT_CH0_DATA, ((uint8_t)reload_u32));
    port_outb(PORT_CH0_DATA, ((uint8_t)(reload_u32 >> 8)));

    gb_initialized = true;
}

uint64_t pit_counter_ms(void) {
    return g_counter_ms;
}

void pit_delay_ms(uint32_t delay_ms) {
    const uint64_t stop_at = g_counter_ms + delay_ms;
    while (g_counter_ms < stop_at) {
        __asm__ volatile("pause" ::: "memory");
    }
}

void pit_irq_handler(void) {
    if (!gb_initialized) {
        panic_enter();
        kprintf("pit: IRQ0 handler was called before initialization\n");
        panic("unexpected behavior");
    }

    g_counter_ms += PIT_PERIOD_MS;
    lapic_send_eoi();
}
