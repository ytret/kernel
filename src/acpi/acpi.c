#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/acpi_defs.h"
#include "heap.h"
#include "kprintf.h"
#include "memfun.h"

static acpi_rsdp1_t *g_rsdp1;

static bool prv_acpi_find_rsdp_bios(uint32_t *out_addr);
static void prv_acpi_dump_rsdp1(acpi_rsdp1_t *rsdp1);

void acpi_init(void) {
    uint32_t rsdp_addr;
    const bool found_rsdp = prv_acpi_find_rsdp_bios(&rsdp_addr);
    if (found_rsdp) {
        kprintf("acpi: found RSD PTR at 0x%08X\n", rsdp_addr);
    } else {
        kprintf("acpi: could not find RSD PTR\n");
        return;
    }

    g_rsdp1 = heap_alloc(sizeof(*g_rsdp1));
    kmemcpy(g_rsdp1, (void *)rsdp_addr, sizeof(*g_rsdp1));
    prv_acpi_dump_rsdp1(g_rsdp1);
    for (;;) {}
}

/**
 * Finds the Root System Description Pointer (RSDP) in BIOS systems.
 * @param[out] out_addr Variable to write the RSDP address to, if it's found.
 * @returns
 * `true` if @a out_addr has been written to with the RSDP address, otherwise
 * `false`.
 */
static bool prv_acpi_find_rsdp_bios(uint32_t *out_addr) {
    /*
     * Section 5.2.5.1 on the process of finding RSDP on BIOS systems says:
     * - The first 1 KB of the extended BIOS area (not used here).
     * - The BIOS read-only memory space between 0x0E0000 and 0x0FFFFF.
     */
    bool found = false;
    const uint8_t *it_u8 = (uint8_t *)0xE0000;
    const uint8_t *const ptr_end = (uint8_t *)(0xFFFFF - 8);
    while (it_u8 < ptr_end) {
        const char *const it_str = (const char *)it_u8;
        const char signature[8] = "RSD PTR ";
        if (!kmemcmp(it_str, signature, sizeof(signature))) {
            *out_addr = (uint32_t)it_u8;
            found = true;
            break;
        }
        it_u8++;
    }
    return found;
}

static void prv_acpi_dump_rsdp1(acpi_rsdp1_t *rsdp1) {
    kprintf("acpi: dump of RSDP 1.0 at %p\n", rsdp1);

    char signature_str[9] = {0};
    char oemid_str[7] = {0};

    kmemcpy(signature_str, rsdp1->signature, sizeof(rsdp1->signature));
    kmemcpy(oemid_str, rsdp1->oemid, sizeof(rsdp1->oemid));
    kprintf("acpi: RSDT 1.0: signature: %s\n", signature_str);
    kprintf("acpi: RSDT 1.0: checksum: 0x%02X\n", rsdp1->checksum);
    kprintf("acpi: RSDT 1.0: oemid: %s\n", oemid_str);
    kprintf("acpi: RSDT 1.0: revision: %u\n", rsdp1->revision);
    kprintf("acpi: RSDT 1.0: rsdt_addr: 0x%08X\n", rsdp1->rsdt_addr);
}
