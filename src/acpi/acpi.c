#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/acpi_defs.h"
#include "heap.h"
#include "kprintf.h"
#include "memfun.h"

static acpi_rsdp1_t *g_rsdp1;
static acpi_rsdt_t *g_rsdt;

static bool prv_acpi_find_rsdp_bios(uint32_t *out_addr);
static bool prv_acpi_check_sum(const void *p_table, size_t table_size);
static void prv_acpi_dump_rsdp1(const acpi_rsdp1_t *rsdp1);
static void prv_acpi_dump_sdt(const acpi_sdt_hdr_t *sdt_hdr);

void acpi_init(void) {
    // Find the RSDP, copy it into the heap, and check its sum.
    uint32_t rsdp_addr;
    const bool found_rsdp = prv_acpi_find_rsdp_bios(&rsdp_addr);
    if (!found_rsdp) {
        kprintf("acpi: could not find RSD PTR\n");
        return;
    }
    kprintf("acpi: found RSD PTR at 0x%08X\n", rsdp_addr);
    prv_acpi_dump_rsdp1((void *)rsdp_addr);

    g_rsdp1 = heap_alloc(sizeof(*g_rsdp1));
    kmemcpy(g_rsdp1, (void *)rsdp_addr, sizeof(*g_rsdp1));
    if (!prv_acpi_check_sum(g_rsdp1, sizeof(*g_rsdp1))) {
        kprintf("acpi: bad checksum of RSDT at 0x%08x\n", rsdp_addr);
        return;
    }

    // Check the RSDT's sum and copy it into the heap.
    const acpi_rsdt_t *const sys_rsdt = (void *)g_rsdp1->rsdt_addr;
    prv_acpi_dump_sdt(&sys_rsdt->header);
    if (!prv_acpi_check_sum(sys_rsdt, sys_rsdt->header.length)) {
        kprintf("acpi: bad checksum of RSDT at 0x%08X\n", g_rsdp1->rsdt_addr);
        return;
    }

    g_rsdt = heap_alloc(sys_rsdt->header.length);
    kmemcpy(g_rsdt, (void *)g_rsdp1->rsdt_addr, sys_rsdt->header.length);

    // Dump the RSDT entry tables, but copy only some of them.
    const uint32_t num_rsdt_entries =
        (g_rsdt->header.length - sizeof(g_rsdt->header)) / 4;
    kprintf("acpi: number of RSDT entries: %u\n", num_rsdt_entries);
    for (uint32_t idx = 0; idx < num_rsdt_entries; idx++) {
        const uint32_t tbl_addr = g_rsdt->entries[idx];
        const acpi_sdt_hdr_t *const sdt = (void *)tbl_addr;
        kprintf("acpi: dump of RSDT entry %u\n", idx);
        prv_acpi_dump_sdt(sdt);
    }
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

static bool prv_acpi_check_sum(const void *p_table, size_t table_size) {
    const uint8_t *const table_u8 = p_table;
    uint8_t sum = 0;
    for (size_t idx = 0; idx < table_size; idx++) {
        sum += table_u8[idx];
    }
    return sum == 0;
}

static void prv_acpi_dump_rsdp1(const acpi_rsdp1_t *rsdp1) {
    char signature_str[9] = {0};
    char oemid_str[7] = {0};
    kmemcpy(signature_str, rsdp1->signature, sizeof(rsdp1->signature));
    kmemcpy(oemid_str, rsdp1->oem_id, sizeof(rsdp1->oem_id));

    kprintf("acpi: RSDP 1.0 at %p: \"%s\", sum 0x%02X, OEM \"%s\" rev. %u, "
            "RSDT at 0x%08X\n",
            rsdp1, signature_str, rsdp1->checksum, oemid_str, rsdp1->revision,
            rsdp1->rsdt_addr);
}

static void prv_acpi_dump_sdt(const acpi_sdt_hdr_t *sdt_hdr) {
    char signature_str[5] = {0};
    char oem_id_str[7] = {0};
    char oem_table_id_str[9] = {0};
    char creator_id_str[5] = {0};
    kmemcpy(signature_str, sdt_hdr->signature, sizeof(sdt_hdr->signature));
    kmemcpy(oem_id_str, sdt_hdr->oem_id, sizeof(sdt_hdr->oem_id));
    kmemcpy(oem_table_id_str, sdt_hdr->oem_table_id,
            sizeof(sdt_hdr->oem_table_id));
    kmemcpy(creator_id_str, sdt_hdr->creator_id, sizeof(sdt_hdr->creator_id));

    kprintf("acpi: SDT at %p: \"%s\", %u bytes, rev. %u, sum 0x%02X, OEM "
            "\"%s\" table "
            "\"%s\" rev. %u, creator \"%s\" rev. %u\n",
            sdt_hdr, signature_str, sdt_hdr->length, sdt_hdr->revision,
            sdt_hdr->checksum, oem_id_str, oem_table_id_str,
            sdt_hdr->oem_revision, creator_id_str, sdt_hdr->creator_revision);
}
