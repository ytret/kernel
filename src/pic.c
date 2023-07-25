#include <kbd.h>
#include <pic.h>
#include <pit.h>
#include <port.h>
#include <printf.h>

#include <stdint.h>

#define PORT_MASTER_CMD         0x20
#define PORT_MASTER_DATA        0x21
#define PORT_SLAVE_CMD          0xA0
#define PORT_SLAVE_DATA         0xA1

#define CMD_EOI                 0x20

#define ICW1                    (1 << 4)
#define ICW1_ICW4_NEEDED        (1 << 0)

#define ICW4_MODE_8086          (1 << 0)

#define OCW3                    (1 << 3)
#define OCW3_READ_REG           (1 << 1)
#define OCW3_ISR                (1 << 0)

static void send_master_cmd(uint8_t cmd);
static void send_master_data(uint8_t data);
static void send_slave_cmd(uint8_t cmd);
static void send_slave_data(uint8_t data);

static uint16_t read_isr(void);

void
pic_init (void)
{
    // ICW1 - start the init sequence.
    //
    uint8_t icw1 = (ICW1 | ICW1_ICW4_NEEDED);
    send_master_cmd(icw1);
    send_slave_cmd(icw1);

    // ICW2 - set the vector offsets.
    //
    send_master_data(32);
    send_slave_data(40);

    // ICW3 - set up cascade mode.
    //
    send_master_data(0b0100);   // master's IRQ2 is connected to the slave
    send_slave_data(2);         // slave's cascade ID

    // ICW4 - enable 8086/8088 mode.
    //
    send_master_data(ICW4_MODE_8086);
    send_slave_data(ICW4_MODE_8086);

    // Mask all IRQs but the slave PIC's IRQ.
    //
    pic_mask_all();
    pic_set_mask(2, false);
}

void
pic_mask_all (void)
{
    for (int i = 0; i < 16; i++)
    {
        pic_set_mask(i, true);
    }
}

void
pic_set_mask (int irq, bool b_mask)
{
    uint16_t port;
    if (irq < 8)
    {
        port = PORT_MASTER_DATA;
    }
    else
    {
        port = PORT_SLAVE_DATA;
        irq -= 8;
    }

    uint8_t byte = port_inb(port);
    if (b_mask)
    {
        byte |= (1 << irq);
    }
    else
    {
        byte &= ~(1 << irq);
    }

    port_outb(port, byte);
}

void
pic_send_eoi (int irq)
{
    if (irq >= 8)
    {
        send_slave_cmd(CMD_EOI);
    }
    send_master_cmd(CMD_EOI);
}

void
pic_spurious_irq_handler (int irq)
{
    uint16_t isr = read_isr();

    if (0 == isr)
    {
        return;
    }

    if (7 == irq)
    {
        // Spurious IRQ from master PIC.
        //
        if (isr & (1 << 0))
        {
            pit_irq_handler();
        }
        else if (isr & (1 << 1))
        {
            kbd_irq_handler();
        }
        else
        {
            printf("pic: unknown spurious IRQ from master PIC\n");
            printf("pic: ISR = 0x%04X\n", isr);
        }
    }
    else
    {
        // Spurious IRQ from slave PIC.
        //
        printf("pic: unknown spurious IRQ from slave PIC\n");
        printf("pic: ISR = 0x%04X\n", isr);

        send_master_cmd(CMD_EOI);
    }
}

static void
send_master_cmd (uint8_t cmd)
{
    port_outb(PORT_MASTER_CMD, cmd);
}

static void
send_master_data (uint8_t data)
{
    port_outb(PORT_MASTER_DATA, data);
}

static void
send_slave_cmd (uint8_t cmd)
{
    port_outb(PORT_SLAVE_CMD, cmd);
}

static void
send_slave_data (uint8_t data)
{
    port_outb(PORT_SLAVE_DATA, data);
}

static uint16_t
read_isr (void)
{
    port_outb(PORT_MASTER_CMD, (OCW3 | OCW3_READ_REG | OCW3_ISR));
    uint8_t isr_master = port_inb(PORT_MASTER_CMD);

    port_outb(PORT_SLAVE_CMD, (OCW3 | OCW3_READ_REG | OCW3_ISR));
    uint8_t isr_slave = port_inb(PORT_SLAVE_CMD);

    return ((isr_slave << 8) | isr_master);
}
