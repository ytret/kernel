#include <panic.h>
#include <pic.h>
#include <pit.h>
#include <port.h>
#include <printf.h>

#define PORT_CMD                0x43
#define PORT_CH0_DATA           0x40

#define REG_CH0                 (0 << 6)
#define REG_BOTH_BYTES          (3 << 4)
#define REG_SQUARE_WAVE         (3 << 1)

#define BASE_FREQ_KHZ            1193

static volatile bool b_initialized;

void
pit_init (uint8_t period_ms)
{
    // Calculate the reload value.
    //
    uint32_t reload_u32 = (BASE_FREQ_KHZ * period_ms);
    if (reload_u32 > 65535)
    {
        printf("PIT: reload value (%u) for period_ms = %u is too big\n",
               reload_u32, period_ms);
        printf("It must be less than or equal to 65535\n");
        panic("pit_init() failed");
    }

    // Send the mode/command register.
    //
    uint8_t reg_byte = (REG_SQUARE_WAVE | REG_BOTH_BYTES | REG_CH0);
    port_outb(PORT_CMD, reg_byte);

    // Send the reload value.
    //
    port_outb(PORT_CH0_DATA, ((uint8_t) reload_u32));
    port_outb(PORT_CH0_DATA, ((uint8_t) (reload_u32 >> 8)));

    b_initialized = true;
}

void
pit_irq0_handler (void)
{
    if (!b_initialized)
    {
        printf("PIT: IRQ0 handler was called before initialization\n");
        panic("unexpected behavior");
    }

    printf(".");
    pic_send_eoi(0);
}
