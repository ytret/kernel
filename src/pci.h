#pragma once

#include <stdint.h>

typedef struct
{
} pci_config_t;

void pci_walk(void);
void pci_dump_config(uint8_t bus, uint8_t dev);
