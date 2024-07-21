#include <stddef.h>
#include <stdint.h>

#include "ahci.h"
#include "kprintf.h"
#include "pci.h"
#include "port.h"

#define PORT_CONFIG_ADDR 0x0CF8
#define PORT_CONFIG_DATA 0x0CFC

#define CONFIG_ADDR_ENABLE      (1 << 31)
#define CONFIG_ADDR_BUS_POS     16
#define CONFIG_ADDR_BUS_MASK    (0xFF << CONFIG_ADDR_BUS_POS)
#define CONFIG_ADDR_DEV_POS     11
#define CONFIG_ADDR_DEV_MASK    (0x1F << CONFIG_ADDR_DEV_POS)
#define CONFIG_ADDR_FUN_POS     8
#define CONFIG_ADDR_FUN_MASK    (0x03 << CONFIG_ADDR_FUN_POS)
#define CONFIG_ADDR_OFFSET_POS  0
#define CONFIG_ADDR_OFFSET_MASK (0xFF << CONFIG_ADDR_OFFSET_POS)

#define CLASS_MASS_STORAGE 0x01
#define SUBCLASS_SATA      0x06

static uint32_t read_config_u32(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset);
static uint16_t read_config_u16(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset);
static uint8_t read_config_u8(uint8_t bus, uint8_t dev, uint8_t fun,
                              uint8_t offset);

void pci_init(void) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint16_t vendor_id = read_config_u16(0, dev, 0, 0x00);
        if (0xFFFF != vendor_id) { pci_init_device(0, dev); }
    }
}

bool pci_init_device(uint8_t bus, uint8_t dev) {
    uint8_t p_config_u8[PCI_CONFIG_SIZE];
    pci_read_config(bus, dev, p_config_u8);

    pci_config_t *p_config = ((pci_config_t *)p_config_u8);

    // Check the header type.
    if (p_config->header_type != 0x00) {
        kprintf("pci: unknown header type %u\n", p_config->header_type);
        return (false);
    }

    // AHCI.
    if ((CLASS_MASS_STORAGE == p_config->base_class) &&
        (SUBCLASS_SATA == p_config->subclass)) {
        kprintf("pci: bus %u device %u: mass storage device, SATA,", bus, dev);

        if (0x01 == p_config->prog_intf) {
            kprintf(" AHCI HBA (major rev. 1)\n");
            bool b_ok = ahci_init(bus, dev);
            if (!b_ok) {
                kprintf("pci: failed to initialize bus %u device %u\n", bus,
                        dev);
            }
            return (b_ok);
        } else {
            kprintf(" unknown programming interface\n");
        }
    } else {
        kprintf("pci: ignoring unknown bus %u device %u\n", bus, dev);
    }

    return (false);
}

void pci_read_config(uint8_t bus, uint8_t dev, void *p_config) {
    __builtin_memset(p_config, 0, sizeof(*p_config));

    uint32_t *p_config_u32 = ((uint32_t *)p_config);
    for (size_t idx = 0; idx < (PCI_CONFIG_SIZE / 4); idx++) {
        p_config_u32[idx] = read_config_u32(bus, dev, 0, (4 * idx));
    }
}

void pci_list_devices(void) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint16_t vendor_id = read_config_u16(0, dev, 0, 0x00);
        uint16_t device_id = read_config_u16(0, dev, 0, 0x02);

        if (0xFFFF == vendor_id) { continue; }

        kprintf("dev %u vendor id %02x device id %02x\n", dev, vendor_id,
                device_id);
    }
}

void pci_dump_config(uint8_t bus, uint8_t dev) {
    size_t const row_bytes = 24;

    for (size_t row = 0; row < (256 / row_bytes); row++) {
        kprintf("%02x  ", (row * row_bytes));

        for (size_t col = 0; col < row_bytes; col++) {
            if ((col > 0) && ((col % 8) == 0)) { kprintf(" "); }

            uint8_t idx = ((row * row_bytes) + col);
            kprintf("%02x ", read_config_u8(bus, dev, 0, idx));
        }

        kprintf("\n");
    }
}

static uint32_t read_config_u32(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset) {
    uint32_t addr =
        (CONFIG_ADDR_ENABLE | (bus << CONFIG_ADDR_BUS_POS) |
         (dev << CONFIG_ADDR_DEV_POS) | (fun << CONFIG_ADDR_FUN_POS) |
         (offset << CONFIG_ADDR_OFFSET_POS));
    port_outl(PORT_CONFIG_ADDR, addr);

    uint32_t reg = port_inl(PORT_CONFIG_DATA);
    return (reg);
}

static uint16_t read_config_u16(uint8_t bus, uint8_t dev, uint8_t fun,
                                uint8_t offset) {
    uint32_t reg = read_config_u32(bus, dev, fun, (offset & (~0x3)));
    return ((uint16_t)(reg >> (8 * (offset & 0x3))));
}

static uint8_t read_config_u8(uint8_t bus, uint8_t dev, uint8_t fun,
                              uint8_t offset) {
    uint32_t reg = read_config_u32(bus, dev, fun, (offset & (~0x3)));
    return ((uint8_t)(reg >> (8 * (offset & 0x3))));
}
