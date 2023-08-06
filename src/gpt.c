#include <ahci.h>
#include <gpt.h>
#include <heap.h>
#include <printf.h>

typedef struct
__attribute__ ((packed))
{
    uint64_t sig;
    uint32_t gpt_rev;
    uint32_t hdr_size;
    uint32_t hdr_cksum;
    uint32_t _reserved_1;

    uint64_t lba_this_hdr;
    uint64_t lba_alt_hdr;

    uint64_t first_usable_block;
    uint64_t last_usable_block;

    uint8_t  p_disk_guid[16];

    uint64_t ptes_lba;
    uint32_t ptes_num;
    uint32_t pte_size;
    uint32_t ptes_cksum;
} pt_hdr_t;

typedef struct
__attribute__ ((packed))
{
    uint8_t  p_type_guid[16];
    uint8_t  p_part_guid[16];

    uint64_t lba_start;
    uint64_t lba_end;

    uint64_t attr;
    uint8_t  p_part_name[];
} pte_t;

_Static_assert(0x5C == sizeof(pt_hdr_t));
_Static_assert(0x38 == sizeof(pte_t));

static uint8_t const gp_root_guid[16] = {
    0xBC, 0x6E, 0x51, 0xF0, 0x9E, 0x2D, 0x06, 0x42,
    0xAB, 0xFC, 0xB1, 0x4E, 0xC7, 0xA6, 0x26, 0xCE,
};

static bool is_pte_used(pte_t const * p_pte);
static void guid_print(uint8_t const p_guid[16]);
static bool guid_equals(uint8_t const p_guid_a[16], uint8_t const p_guid_b[16]);

bool
gpt_find_root_part (uint64_t * p_lba_start, uint64_t * p_lba_end)
{
    // Zero out the output variables.
    //
    *p_lba_start = 0;
    *p_lba_end   = 0;

    // Ensure the root disk was found.
    //
    if (!ahci_is_init())
    {
        printf("gpt: AHCI driver is not initialized\n");
        return (false);
    }

    // Read the partition table header.
    //
    pt_hdr_t * p_hdr = heap_alloc(512);
    ahci_read_sectors(1, 1, p_hdr);

    char p_sig[9] = { 0 };
    __builtin_memcpy(p_sig, p_hdr, 8);

    printf("gpt: signature '%s'\n", p_sig);
    printf("gpt: GPT revision 0x%08x\n", p_hdr->gpt_rev);
    printf("gpt: header size %u\n", p_hdr->hdr_size);
    printf("gpt: header cksum 0x%08x\n", p_hdr->hdr_cksum);
    printf("gpt: LBA of this header %u\n", p_hdr->lba_this_hdr);
    printf("gpt: LBA of alternate header %u\n", p_hdr->lba_alt_hdr);
    printf("gpt: disk GUID ");
    guid_print(p_hdr->p_disk_guid);
    printf("\n");
    printf("gpt: PTE array starts at LBA %u\n", p_hdr->ptes_lba);
    printf("gpt: PTE array length %u entries\n", p_hdr->ptes_num);
    printf("gpt: PTE size %u\n", p_hdr->pte_size);
    printf("gpt: PTE array cksum 0x%08x\n", p_hdr->ptes_cksum);

    // Read all the partition entries.
    //
    size_t ptes_sectors = (((p_hdr->ptes_num * p_hdr->pte_size) + 511) / 512);
    uint8_t * p_ptes_u8 = heap_alloc(512 * ptes_sectors);
    bool b_read = ahci_read_sectors(p_hdr->ptes_lba, ptes_sectors, p_ptes_u8);
    if (!b_read)
    {
        printf("gpt: AHCI read failed\n");
        return (false);
    }

    // Iterate through the partition entries.
    //
    bool b_found = false;
    for (size_t idx = 0; idx < p_hdr->ptes_num; idx++)
    {
        pte_t const * p_pte =
            (pte_t const *) (p_ptes_u8 + (idx * p_hdr->pte_size));

        if (is_pte_used(p_pte))
        {
            size_t name_len = (p_hdr->pte_size - sizeof(pte_t));
            char * p_name   = heap_alloc(name_len);
            __builtin_memcpy(p_name, p_pte->p_part_name, name_len);

            printf("gpt: partition %u: type ", idx);
            guid_print(p_pte->p_type_guid);
            printf(", start 0x%08x%08x, end 0x%08x%08x, attr 0x%08x%08x"
                   ", name '%s'\n",
                   (uint32_t) (p_pte->lba_start >> 32),
                   (uint32_t) p_pte->lba_start,
                   (uint32_t) (p_pte->lba_end >> 32),
                   (uint32_t) p_pte->lba_end,
                   (uint32_t) (p_pte->attr >> 8),
                   (uint32_t) (p_pte->attr & 0xFFFFFFFF),
                   p_name);

            if (guid_equals(p_pte->p_type_guid, gp_root_guid))
            {
                printf("gpt: found root parttion\n");
                b_found      = true;
                *p_lba_start = p_pte->lba_start;
                *p_lba_end   = p_pte->lba_end;
            }
        }
    }

    return (b_found);
}

static bool
is_pte_used (pte_t const * p_pte)
{
    for (size_t idx = 0; idx < 16; idx++)
    {
        if (0 != p_pte->p_type_guid[idx])
        {
            return (true);
        }
    }
    return (false);
}

static void
guid_print (uint8_t const p_guid[16])
{
    uint32_t * p_one   = (uint32_t *) &p_guid[0];   // 0..3
    uint16_t * p_two   = (uint16_t *) &p_guid[4];   // 4..5
    uint16_t * p_three = (uint16_t *) &p_guid[6];   // 6..7
    uint8_t  * p_four  = (uint8_t *) &p_guid[8];    // 8
    uint8_t  * p_five  = (uint8_t *) &p_guid[9];    // 9

    printf("%08X-%04X-%04X-%02X%02X-",
           *p_one, *p_two, *p_three, *p_four, *p_five);

    for (size_t idx = 10; idx < 16; idx++)
    {
        printf("%02X", p_guid[idx]);
    }
}

static bool
guid_equals (uint8_t const p_guid_a[16], uint8_t const p_guid_b[16])
{
    for (size_t idx = 0; idx < 16; idx++)
    {
        if (p_guid_a[idx] != p_guid_b[idx])
        {
            return (false);
        }
    }
    return (true);
}
