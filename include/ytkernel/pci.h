/**
 * @file pci.h
 * PCI driver API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/// Number of PCI buses to enumerate in #pci_init().
#define PCI_ENUM_BUSES 1
static_assert(PCI_ENUM_BUSES <= 256, "there cannot be more than 256 buses");

/**
 * Maximum number of devices that can be connected to a single PCI bus.
 * This value (32) is determined by the number of bits (5) provided for device
 * selection in CAS addresses.
 */
#define PCI_DEVS_PER_BUS 32

/**
 * Maximum number of functions per PCI device.
 * This value (8) is determined by the nubmer of bits (3) provided for function
 * selection in CAS addresses.
 */
#define PCI_FUNS_PER_DEV 8

/**
 * Common fields of the various types of PCI configuration headers.
 * Refer to section 6.1 Configuration Space Organization.
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;

    union {
        uint16_t command;
        struct [[gnu::packed]] {
            uint32_t io_space : 1;
            uint32_t mem_space : 1;
            uint32_t bus_master : 1;
            uint32_t special_cycles : 1;
            uint32_t mem_write_invalidate_en : 1;
            uint32_t vga_palette_snoop : 1;
            uint32_t parity_err_response : 1;
            uint32_t reserved1 : 1;
            uint32_t serr_en : 1;
            uint32_t fast_b2b_en : 1;
            uint32_t interrupt_disable : 1;
            uint32_t reserved2 : 5;
        } command_bit;
    };

    union {
        uint16_t status;
        struct [[gnu::packed]] {
            uint32_t reserved1 : 3;
            uint32_t interrupt_status : 1;
            uint32_t cap_list : 1;
            uint32_t cap_66mhz : 1;
            uint32_t reserved2 : 1;
            uint32_t cap_fast_b2b : 1;
            uint32_t master_data_parity_err : 1;
            uint32_t devsel_timing : 2;
            uint32_t signaled_target_abort : 1;
            uint32_t received_target_abort : 1;
            uint32_t received_master_abort : 1;
            uint32_t signaled_system_err : 1;
            uint32_t detected_parity_err : 1;
        } status_bit;
    };

    uint8_t revision_id;

    uint8_t interface;
    uint8_t sub_class;
    uint8_t base_class;

    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;

    /// Built-in Self Test.
    union {
        uint8_t bist;
        struct [[gnu::packed]] {
            uint32_t completion_code : 4;
            uint32_t reserved1 : 2;
            uint32_t start_bist : 1;
            uint32_t cap_bist : 1;
        } bist_bit;
    };
} pci_header_common_t;

/**
 * Type 00h Configuration Space Header.
 * Refer to section 6.1 Configuration Space Organization.
 */
typedef struct {
    pci_header_common_t common;

    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;
    uint32_t cardbus_cis_ptr;

    uint16_t subsys_vendor_id;
    uint16_t subsys_id;

    uint32_t expansion_rom_base_addr;

    uint8_t cap_ptr;
    uint32_t reserved1 : 24;

    uint32_t reserved2;

    uint8_t int_line;
    uint8_t int_pin;
    uint8_t min_gnt;
    uint8_t max_lat;
} pci_header_00h_t;

/**
 * Base class codes.
 * Refer to Appendix D Class Codes.
 */
typedef enum {
    PCI_BASE_CLASS_NONE = 0x00,
    PCI_BASE_CLASS_MASS_STORAGE = 0x01,
    PCI_BASE_CLASS_NETWORK_CTRL = 0x02,
    PCI_BASE_CLASS_DISPLAY_CTRL = 0x03,
    PCI_BASE_CLASS_MULTIMEDIA_DEVICE = 0x04,
    PCI_BASE_CLASS_MEMORY_CTRL = 0x05,
    PCI_BASE_CLASS_BRIDGE_DEVICE = 0x06,
    PCI_BASE_CLASS_SIMPLE_COMM_CTRL = 0x07,
    PCI_BASE_CLASS_BASE_SYSTEM_PERIPH = 0x08,
    PCI_BASE_CLASS_INPUT_DEVICE = 0x09,
    PCI_BASE_CLASS_DOCKING_STATION = 0x0A,
    PCI_BASE_CLASS_PROCESSOR = 0x0B,
    PCI_BASE_CLASS_SERIAL_BUS_CTRL = 0x0C,
    PCI_BASE_CLASS_WIRELESS_CTRL = 0x0D,
    PCI_BASE_CLASS_INTELLIGENT_IO_CTRL = 0x0E,
    PCI_BASE_CLASS_SATELLITE_COMM_CTRL = 0x0F,
    PCI_BASE_CLASS_ENCRYPTION_CTRL = 0x10,
    PCI_BASE_CLASS_DSP_CTRL = 0x11,
    PCI_BASE_CLASS_OTHER = 0xFF,
} pci_base_class_t;

/**
 * Base class #PCI_BASE_CLASS_MASS_STORAGE sub classes.
 * Refer to section D.2 Base Class 01h.
 */
typedef enum {
    PCI_MASS_STORAGE_SCSI_BUS_CTRL,
    PCI_MASS_STORAGE_IDE_CTRL,
    PCI_MASS_STORAGE_FLOPPY_CTRL,
    PCI_MASS_STORAGE_IPI_BUS_CTRL,
    PCI_MASS_STORAGE_RAID_CTRL,
    PCI_MASS_STORAGE_ATA_CTRL,
    PCI_MASS_STORAGE_SATA_DPA, //!< Serial ATA Direct Port Access (DPA).
    PCI_MASS_STORAGE_OTHER,
} pci_mass_storage_subclass_t;

/// Sub class #PCI_MASS_STORAGE_SATA_DPA interfaces.
typedef enum {
    PCI_SATA_INTERFACE_AHCI = 0x01,
} pci_sata_interface_t;

/// Internal representation of a PCI device.
typedef struct {
    uint8_t bus_num;
    uint8_t dev_num;
    uint8_t fun_num;

    pci_header_00h_t header;
} pci_dev_t;

/// Enumerate PCI devices.
void pci_init(void);

/// Returns the number of devices connected to the host PCI bus.
size_t pci_num_devs(void);

/**
 * Get the PCI device at index @a idx.
 * @param idx Device index, must be less than #pci_num_devs().
 * @returns
 * - A constant pointer to the device information structure.
 * - `NULL` if the index is beyond the limit.
 */
const pci_dev_t *pci_get_dev_const(size_t idx);

/// Prints the device bus, device, function numbers, vendor ID, device ID.
void pci_dump_dev_short(const pci_dev_t *dev);

/// Prints the full header of the device.
void pci_dump_dev_header(const pci_dev_t *dev);
