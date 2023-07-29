#include <ahci.h>
#include <pci.h>
#include <port.h>
#include <printf.h>

#include <stddef.h>
#include <stdint.h>

#define PORT_CONFIG_ADDR        0x0CF8
#define PORT_CONFIG_DATA        0x0CFC

#define CONFIG_ADDR_ENABLE      (1 << 31)
#define CONFIG_ADDR_BUS_POS     16
#define CONFIG_ADDR_BUS_MASK    (0xFF << CONFIG_ADDR_BUS_POS)
#define CONFIG_ADDR_DEV_POS     11
#define CONFIG_ADDR_DEV_MASK    (0x1F << CONFIG_ADDR_DEV_POS)
#define CONFIG_ADDR_FUN_POS     8
#define CONFIG_ADDR_FUN_MASK    (0x03 << CONFIG_ADDR_FUN_POS)
#define CONFIG_ADDR_OFFSET_POS  0
#define CONFIG_ADDR_OFFSET_MASK (0xFF << CONFIG_ADDR_OFFSET_POS)

static uint32_t read_config_u32(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset);
static uint16_t read_config_u16(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset);
static uint8_t  read_config_u8(uint8_t bus, uint8_t dev, uint8_t fun,
                               uint8_t offset);

void
pci_walk (void)
{
    for (uint8_t dev = 0; dev < 32; dev++)
    {
        uint16_t vendor_id = read_config_u16(0, dev, 0, 0x0);
        uint16_t device_id = read_config_u16(0, dev, 0, 0x2);

        if (0xFFFF == vendor_id)
        {
            continue;
        }

        printf("dev %u vendor id %02x device id %02x\n", dev, vendor_id,
               device_id);

        if ((0x8086 == vendor_id) && (0x2922 == device_id))
        {
            ahci_init(0, dev);
        }
    }
}

void
pci_dump_config (uint8_t bus, uint8_t dev)
{
    size_t const row_bytes = 24;

    for (size_t row = 0; row < (256 / row_bytes); row++)
    {
        printf("%02x  ", (row * row_bytes));

        for (size_t col = 0; col < row_bytes; col++)
        {
            if ((col > 0) && ((col % 8) == 0))
            {
                printf(" ");
            }

            uint8_t idx = ((row * row_bytes) + col);
            printf("%02x ", read_config_u8(bus, dev, 0, idx));
        }

        printf("\n");
    }
}

static uint32_t
read_config_u32 (uint8_t bus, uint8_t dev, uint8_t fun, uint8_t offset)
{
    uint32_t addr = (CONFIG_ADDR_ENABLE
                     | (bus << CONFIG_ADDR_BUS_POS)
                     | (dev << CONFIG_ADDR_DEV_POS)
                     | (fun << CONFIG_ADDR_FUN_POS)
                     | (offset << CONFIG_ADDR_OFFSET_POS));
    port_outl(PORT_CONFIG_ADDR, addr);

    uint32_t reg = port_inl(PORT_CONFIG_DATA);
    return (reg);
}

static uint16_t
read_config_u16 (uint8_t bus, uint8_t dev, uint8_t fun, uint8_t offset)
{
    uint32_t reg = read_config_u32(bus, dev, fun, (offset & (~0x3)));
    return ((uint16_t) (reg >> (8 * (offset & 0x3))));
}

static uint8_t
read_config_u8 (uint8_t bus, uint8_t dev, uint8_t fun, uint8_t offset)
{
    uint32_t reg = read_config_u32(bus, dev, fun, (offset & (~0x3)));
    return ((uint8_t) (reg >> (8 * (offset & 0x3))));
}
