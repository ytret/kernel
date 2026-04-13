/**
 * @file pci.c
 * PCI bus driver implementation.
 * Refer to PCI Local Bus Specification, rev 3.0.
 */

#include <stddef.h>
#include <stdint.h>

#include "kinttypes.h"
#include "log.h"
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
    if (!dev) { PANIC("invalid argument 'dev' value NULL"); }
    LOG_DEBUG("%u-%u-%u: %04x:%04x class %02x.%02x.%02x", dev->bus_num,
              dev->dev_num, dev->fun_num, dev->header.common.vendor_id,
              dev->header.common.device_id, dev->header.common.base_class,
              dev->header.common.sub_class, dev->header.common.interface);
}

void pci_dump_dev_header(const pci_dev_t *dev) {
    if (!dev) { PANIC("invalid argument 'dev' value NULL"); }

    LOG_DEBUG("vendor_id = 0x%04X", dev->header.common.vendor_id);
    LOG_DEBUG("device_id = 0x%04X", dev->header.common.device_id);
    LOG_DEBUG("command = 0x%04X", dev->header.common.command);
    LOG_DEBUG("status = 0x%04X", dev->header.common.status);
    LOG_DEBUG("revision_id = 0x%02X", dev->header.common.revision_id);
    LOG_DEBUG("base_class = 0x%02X", dev->header.common.base_class);
    LOG_DEBUG("sub_class = 0x%02X", dev->header.common.sub_class);
    LOG_DEBUG("interface = 0x%02X", dev->header.common.interface);
    LOG_DEBUG("cacheline_size = 0x%02X", dev->header.common.cacheline_size);
    LOG_DEBUG("latency_timer = 0x%02X", dev->header.common.latency_timer);
    LOG_DEBUG("header_type = 0x%02X", dev->header.common.header_type);
    LOG_DEBUG("bist = 0x%02X", dev->header.common.bist);
    LOG_DEBUG("bar0 = 0x%08" PRIx32, dev->header.bar0);
    LOG_DEBUG("bar1 = 0x%08" PRIx32, dev->header.bar1);
    LOG_DEBUG("bar2 = 0x%08" PRIx32, dev->header.bar2);
    LOG_DEBUG("bar3 = 0x%08" PRIx32, dev->header.bar3);
    LOG_DEBUG("bar4 = 0x%08" PRIx32, dev->header.bar4);
    LOG_DEBUG("bar5 = 0x%08" PRIx32, dev->header.bar5);
    LOG_DEBUG("cardbus_cis_ptr = 0x%08" PRIx32, dev->header.cardbus_cis_ptr);
    LOG_DEBUG("subsys_vendor_id = 0x%04X", dev->header.subsys_vendor_id);
    LOG_DEBUG("subsys_id = 0x%04X", dev->header.subsys_id);
    LOG_DEBUG("expansion_rom_base_addr = 0x%08" PRIx32,
              dev->header.expansion_rom_base_addr);
    LOG_DEBUG("cap_ptr = 0x%02X", dev->header.cap_ptr);
    LOG_DEBUG("int_line = 0x%02X", dev->header.int_line);
    LOG_DEBUG("int_pin = 0x%02X", dev->header.int_pin);
    LOG_DEBUG("min_gnt = 0x%02X", dev->header.min_gnt);
    LOG_DEBUG("max_lat = 0x%02X", dev->header.max_lat);
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
                LOG_ERROR("maximum number of devices (%u) has been reached",
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
                LOG_ERROR("%u-%u-%u: unknown header type 0x%02x", bus_num,
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
