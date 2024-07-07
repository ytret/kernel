#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PCI_CONFIG_SIZE 256

typedef struct __attribute__((packed, aligned(4))) {
    // Type 00h configuration space header.  See PCI Local Bus Specification,
    // rev. 3.0, p. 215.

    // 0x00
    uint16_t vendor_id;
    uint16_t device_id;

    // 0x04
    uint16_t command;
    uint16_t status;

    // 0x08
    uint8_t revision_id;
    uint8_t prog_intf;
    uint8_t subclass;
    uint8_t base_class;

    // 0x0C
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type : 7;
    bool b_multifun : 1;
    uint8_t bist; // built-in self test

    // 0x10
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;

    // 0x28
    uint32_t cardbus_cis_ptr;

    // 0x2C
    uint16_t subsys_vendor_id;
    uint16_t subsys_id;

    // 0x30
    uint32_t exp_rom_base_addr;

    // 0x34
    uint8_t capabiltiies_ptr;
    uint32_t reserved_1 : 24;

    // 0x38
    uint32_t reserved_2;

    // 0x3C
    uint8_t int_line;
    uint8_t int_pin;
    uint8_t min_gnt;
    uint8_t max_lat;
} pci_config_t;

_Static_assert(0x40 == sizeof(pci_config_t), "invalid size of pci_config_t");

void pci_init(void);
bool pci_init_device(uint8_t bus, uint8_t dev);

void pci_read_config(uint8_t bus, uint8_t dev, void *p_config);

void pci_list_devices(void);
void pci_dump_config(uint8_t bus, uint8_t dev);
