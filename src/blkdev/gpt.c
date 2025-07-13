/**
 * @file gpt.c
 * GUID Partition Table (GPT) driver.
 *
 * Refer to Unified Extensible Firmware Interface (UEFI) Specification,
 * Release 2.11.
 */

#include "blkdev/blkdev.h"
#include "blkdev/gpt.h"
#include "heap.h"
#include "kprintf.h"
#include "memfun.h"
#include "string.h"

#define GPT_SIGNATURE 0x5452415020494645 // "EFI PART"

/**
 * GPT Header structure.
 * Refer to section 5.3.2 "GPT Header".
 */
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved1;

    uint64_t my_lba;
    uint64_t alternate_lba;

    uint64_t first_usable_lba;
    uint64_t last_usable_lba;

    uint8_t disk_guid[16];

    uint64_t gpes_lba;   //!< Starting LBA of the GUID Partition Entry Array.
    uint32_t gpes_num;   //!< Number of Partition Entries in the GPE Array.
    uint32_t gpe_size;   //!< Size of each GUID Partition Entry (bytes).
    uint32_t gpes_crc32; //!< GPE Array checksum.
} gpt_hdr_t;

/**
 * GPT Partition Entry structure.
 * Refer to section 5.3.3 "GPT Partition Entry Array".
 */
typedef struct {
    uint8_t type_guid[16]; //!< Partition Type GUID.
    uint8_t part_guid[16]; //!< Unique Partition GUID.

    uint64_t starting_lba;
    uint64_t ending_lba;

    uint64_t attr;
    char part_name[]; //!< NUL-terminated, human-readable partition name.
} gpt_gpe_t;

// F0516EBC-2D9E-4206-ABFC-B14EC7A626CE
[[maybe_unused]]
static constexpr uint8_t gp_root_guid[16] = {
    0xBC, 0x6E, 0x51, 0xF0, 0x9E, 0x2D, 0x06, 0x42,
    0xAB, 0xFC, 0xB1, 0x4E, 0xC7, 0xA6, 0x26, 0xCE,
};

static bool gpt_is_gpe_used(const gpt_gpe_t *gpe);
static void gpt_print_guid(const uint8_t guid[16]);

bool gpt_probe_signature(blkdev_dev_t *dev) {
    uint8_t *const sector1 = heap_alloc(512);
    if (!blkdev_sync_read(dev, 1, 1, sector1)) {
        kprintf("gpt: failed to read sector 1\n");
        heap_free(sector1);
        return false;
    }

    const gpt_hdr_t *const gpt_hdr = (const gpt_hdr_t *)sector1;
    if (gpt_hdr->signature == GPT_SIGNATURE) {
        kprintf("gpt: found a valid GPT signature\n");
        heap_free(sector1);
        return true;
    } else {
        kprintf("gpt: no valid GPT signature\n");
        heap_free(sector1);
        return false;
    }
}

bool gpt_parse(blkdev_dev_t *dev, gpt_disk_t **out_gpt_disk) {
    uint8_t *const sector1 = heap_alloc(512);
    if (!blkdev_sync_read(dev, 1, 1, sector1)) {
        kprintf("gpt: failed to read sector 1\n");
        heap_free(sector1);
        return false;
    }

    const gpt_hdr_t *const gpt_hdr = (const gpt_hdr_t *)sector1;
    if (gpt_hdr->signature != GPT_SIGNATURE) {
        kprintf("gpt: invalid GPT signature: 0x%08X%08X\n",
                (uint32_t)(gpt_hdr->signature >> 32),
                (uint32_t)gpt_hdr->signature);
        heap_free(sector1);
        return false;
    }
    // TODO: check everything listed on page 118.

    kprintf("gpt: Signature '%s'\n", (const char *)&gpt_hdr->signature);
    kprintf("gpt: GPT Revision 0x%08x\n", gpt_hdr->revision);
    kprintf("gpt: Header Size %u\n", gpt_hdr->header_size);
    kprintf("gpt: Header CRC32 0x%08x\n", gpt_hdr->header_crc32);
    kprintf("gpt: My LBA %u\n", gpt_hdr->my_lba);
    kprintf("gpt: Alternate LBA %u\n", gpt_hdr->alternate_lba);
    kprintf("gpt: First Usable LBA %u\n", gpt_hdr->first_usable_lba);
    kprintf("gpt: Last Usable LBA %u\n", gpt_hdr->last_usable_lba);
    kprintf("gpt: Disk GUID ");
    gpt_print_guid(gpt_hdr->disk_guid);
    kprintf("\n");
    kprintf("gpt: GPE Array starts at LBA %u\n", gpt_hdr->gpes_lba);
    kprintf("gpt: GPE Array length %u entries\n", gpt_hdr->gpes_num);
    kprintf("gpt: GPE Size %u\n", gpt_hdr->gpe_size);
    kprintf("gpt: GPE Array CRC32 0x%08x\n", gpt_hdr->gpes_crc32);

    const size_t gpes_sectors =
        ((gpt_hdr->gpe_size * gpt_hdr->gpes_num) + 511) / 512;
    uint8_t *const gpes_buf = heap_alloc(512 * gpes_sectors);
    if (!blkdev_sync_read(dev, gpt_hdr->gpes_lba, gpes_sectors, gpes_buf)) {
        kprintf(
            "gpt: failed to read GUID Partition Entry Array (sectors %u..%u)\n",
            gpt_hdr->gpes_lba, gpt_hdr->gpes_lba + gpes_sectors);
        heap_free(sector1);
        heap_free(gpes_buf);
        return false;
    }

    const size_t num_parts = gpt_hdr->gpes_num;
    gpt_part_t *const parts = heap_alloc(sizeof(gpt_part_t) * num_parts);
    kmemset(parts, 0, sizeof(gpt_part_t) * num_parts);

    gpt_disk_t *const disk = heap_alloc(sizeof(*disk));
    kmemset(disk, 0, sizeof(*disk));
    disk->parts = parts;
    disk->num_parts = num_parts;

    for (size_t idx_part = 0; idx_part < num_parts; idx_part++) {
        const gpt_gpe_t *const gpe =
            (const gpt_gpe_t *)(gpes_buf + gpt_hdr->gpe_size * idx_part);

        const size_t name_len = string_len(gpe->part_name);
        char *const part_name = heap_alloc(name_len + 1);
        kmemcpy(part_name, gpe->part_name, name_len + 1);

        gpt_part_t *const part = &parts[idx_part];
        part->disk = disk;
        part->part_name = part_name;
        part->starting_lba = gpe->starting_lba;
        part->ending_lba = gpe->ending_lba;
        kmemcpy(part->type_guid, gpe->type_guid, 16);
        kmemcpy(part->part_guid, gpe->part_guid, 16);
        part->used = gpt_is_gpe_used(gpe);
    }

    heap_free(sector1);
    heap_free(gpes_buf);

    if (out_gpt_disk) { *out_gpt_disk = disk; }

    return true;
}

static bool gpt_is_gpe_used(const gpt_gpe_t *gpe) {
    const uint8_t unused_type_guid[16] = {0};
    return !kmemcmp(gpe->type_guid, unused_type_guid, 16);
}

static void gpt_print_guid(const uint8_t guid[16]) {
    uint32_t *p_one = (uint32_t *)&guid[0];   // 0..3
    uint16_t *p_two = (uint16_t *)&guid[4];   // 4..5
    uint16_t *p_three = (uint16_t *)&guid[6]; // 6..7
    uint8_t *p_four = (uint8_t *)&guid[8];    // 8
    uint8_t *p_five = (uint8_t *)&guid[9];    // 9

    kprintf("%08X-%04X-%04X-%02X%02X-", *p_one, *p_two, *p_three, *p_four,
            *p_five);

    for (size_t idx = 10; idx < 16; idx++) {
        kprintf("%02X", guid[idx]);
    }
}
