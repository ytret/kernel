#include "pci_arch.h"

#include "arch/x86/port.h"

#define PCI_PORT_CAS_ADDR 0x0CF8
#define PCI_PORT_CAS_DATA 0x0CFC

void pci_arch_cas_read(uint32_t start_addr, void *v_buf, size_t num_dwords) {
    uint32_t *const buf_u32 = v_buf;
    for (size_t cnt_dword = 0; cnt_dword < num_dwords; cnt_dword++) {
        port_outl(PCI_PORT_CAS_ADDR, start_addr + 4 * cnt_dword);
        buf_u32[cnt_dword] = port_inl(PCI_PORT_CAS_DATA);
    }
}
