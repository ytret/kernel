/**
 * @file pci.c
 * PCI bus driver implementation.
 * Refer to PCI Local Bus Specification, rev 3.0.
 */

#include <stddef.h>
#include <stdint.h>

#include "kprintf.h"
#include "panic.h"
#include "pci.h"
#include "port.h"

/**
 * Maximum number of _connected_ devices supported by the driver.
 * This value may be chosen arbitrarily, but it needs to be large enough to
 * cover the most PC configurations.
 */
#define PCI_MAX_DEVS 32

#define PCI_PORT_CAS_ADDR 0x0CF8
#define PCI_PORT_CAS_DATA 0x0CFC

/**
 * Configuration Address Space (CAS) Address register.
 * Refer to section 3.2.2.3.2 Software Generation of Configuration Transactions.
 */
typedef union {
    uint32_t val;
    struct {
        uint32_t reserved1 : 2;
        uint32_t reg_num : 6;
        uint32_t fun_num : 3;
        uint32_t dev_num : 5;
        uint32_t bus_num : 8;
        uint32_t reserved2 : 7;
        uint32_t enable : 1;
    } bit;
} pci_addr_t;

static pci_dev_t g_pci_devs[PCI_MAX_DEVS];
static size_t g_pci_num_devs;

static void prv_pci_enumerate_bus(uint8_t bus_num);
static void prv_pci_cas_read(uint32_t start_addr, void *v_buf,
                             size_t num_dwords);

void pci_init(void) {
    for (size_t bus_num = 0; bus_num < PCI_ENUM_BUSES; bus_num++) {
        prv_pci_enumerate_bus(bus_num);
    }
}

size_t pci_num_devs(void) {
    return g_pci_num_devs;
}

const pci_dev_t *pci_get_dev_const(size_t idx) {
    if (idx < g_pci_num_devs) {
        return &g_pci_devs[idx];
    } else {
        return NULL;
    }
}

void pci_dump_dev_short(const pci_dev_t *dev) {
    if (!dev) { panic("pci_dump_dev_short: dev = NULL"); }
    kprintf("pci: %u-%u-%u: %04x:%04x class %02x.%02x.%02x\n", dev->bus_num,
            dev->dev_num, dev->fun_num, dev->header.common.vendor_id,
            dev->header.common.device_id, dev->header.common.base_class,
            dev->header.common.sub_class, dev->header.common.interface);
}

void pci_dump_dev_header(const pci_dev_t *dev) {
    if (!dev) { panic("pci_dump_dev_short: dev = NULL"); }

    kprintf("vendor_id = 0x%04X\n", dev->header.common.vendor_id);
    kprintf("device_id = 0x%04X\n", dev->header.common.device_id);
    kprintf("command = 0x%04X\n", dev->header.common.command);
    kprintf("status = 0x%04X\n", dev->header.common.status);
    kprintf("revision_id = 0x%02X\n", dev->header.common.revision_id);
    kprintf("base_class = 0x%02X\n", dev->header.common.base_class);
    kprintf("sub_class = 0x%02X\n", dev->header.common.sub_class);
    kprintf("interface = 0x%02X\n", dev->header.common.interface);
    kprintf("cacheline_size = 0x%02X\n", dev->header.common.cacheline_size);
    kprintf("latency_timer = 0x%02X\n", dev->header.common.latency_timer);
    kprintf("header_type = 0x%02X\n", dev->header.common.header_type);
    kprintf("bist = 0x%02X\n", dev->header.common.bist);
    kprintf("bar0 = 0x%08X\n", dev->header.bar0);
    kprintf("bar1 = 0x%08X\n", dev->header.bar1);
    kprintf("bar2 = 0x%08X\n", dev->header.bar2);
    kprintf("bar3 = 0x%08X\n", dev->header.bar3);
    kprintf("bar4 = 0x%08X\n", dev->header.bar4);
    kprintf("bar5 = 0x%08X\n", dev->header.bar5);
    kprintf("cardbus_cis_ptr = 0x%08X\n", dev->header.cardbus_cis_ptr);
    kprintf("subsys_vendor_id = 0x%04X\n", dev->header.subsys_vendor_id);
    kprintf("subsys_id = 0x%04X\n", dev->header.subsys_id);
    kprintf("expansion_rom_base_addr = 0x%08X\n",
            dev->header.expansion_rom_base_addr);
    kprintf("cap_ptr = 0x%02X\n", dev->header.cap_ptr);
    kprintf("int_line = 0x%02X\n", dev->header.int_line);
    kprintf("int_pin = 0x%02X\n", dev->header.int_pin);
    kprintf("min_gnt = 0x%02X\n", dev->header.min_gnt);
    kprintf("max_lat = 0x%02X\n", dev->header.max_lat);
}

/**
 * Enumerates bus number @a bus_num and adds connected devices to #g_pci_devs.
 * @param bus_num PCI bus to enumerate (0..255).
 */
static void prv_pci_enumerate_bus(uint8_t bus_num) {
    pci_header_common_t header;
    for (uint8_t dev_num = 0; dev_num < PCI_DEVS_PER_BUS; dev_num++) {
        for (uint8_t fun_num = 0; fun_num < PCI_FUNS_PER_DEV; fun_num++) {
            if (g_pci_num_devs == PCI_MAX_DEVS) {
                kprintf(
                    "pci: maximum number of devices (%u) has been reached\n",
                    PCI_MAX_DEVS);
                break;
            }
            pci_dev_t *const dev = &g_pci_devs[g_pci_num_devs];

            pci_addr_t addr = {0};
            addr.bit.enable = 1;
            addr.bit.bus_num = bus_num;
            addr.bit.dev_num = dev_num;
            addr.bit.fun_num = fun_num;
            addr.bit.reg_num = 0;

            // First, we read the common part of the header, to see if a device
            // is connected.
            static_assert(sizeof(header) % 4 == 0, "invalid header structure");
            prv_pci_cas_read(addr.val, &header, sizeof(header) / 4);

            if (header.vendor_id == 0xFFFF) { continue; }
            if (header.header_type != 0x00) {
                kprintf("pci: %u-%u-%u: unknown header type 0x%02x\n", bus_num,
                        dev_num, fun_num, header.header_type);
                continue;
            }

            dev->bus_num = bus_num;
            dev->dev_num = dev_num;
            dev->fun_num = fun_num;
            dev->header.common = header;

            // Second, we read the rest of the type 00h header.
            addr.bit.reg_num = sizeof(pci_header_common_t) / 4;
            static_assert(
                (sizeof(pci_header_00h_t) - sizeof(pci_header_common_t)) % 4 ==
                    0,
                "invalid header 00h structure");
            prv_pci_cas_read(
                addr.val,
                (void *)((uint32_t)&dev->header + sizeof(pci_header_common_t)),
                (sizeof(pci_header_00h_t) - sizeof(pci_header_common_t)) / 4);

            g_pci_num_devs++;
        }

        if (g_pci_num_devs == PCI_MAX_DEVS) { break; }
    }
}

/**
 * Reads dwords from the CAS starting at address @a addr into @a buf.
 * @param start_addr Start address.
 * @param v_buf      Buffer to copy bytes into.
 * @param num_dwords Number of dwords to read.
 */
static void prv_pci_cas_read(uint32_t start_addr, void *v_buf,
                             size_t num_dwords) {
    uint32_t *const buf_u32 = v_buf;
    for (size_t cnt_dword = 0; cnt_dword < num_dwords; cnt_dword++) {
        port_outl(PCI_PORT_CAS_ADDR, start_addr + 4 * cnt_dword);
        buf_u32[cnt_dword] = port_inl(PCI_PORT_CAS_DATA);
    }
}
