#pragma once

#include <stdint.h>

void acpi_init(void);

/**
 * Copies the local APIC register map start address to @a *out_addr.
 * @param[out] out_addr Pointer to the variable which will store the address.
 * @returns
 * `true` if MADT has been found previously during #acpi_init() and the LAPIC
 * address has been copied to @a *out_addr, otherwise `false`.
 */
bool acpi_get_lapic_addr(uint32_t *out_addr);
