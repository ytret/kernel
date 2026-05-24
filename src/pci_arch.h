#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Reads dwords from the CAS starting at address @a addr into @a buf.
 * @param start_addr Start address.
 * @param v_buf      Buffer to copy bytes into.
 * @param num_dwords Number of dwords to read.
 */
void pci_arch_cas_read(uint32_t start_addr, void *v_buf, size_t num_dwords);
