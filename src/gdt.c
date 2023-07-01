#include <gdt.h>

#define ENTRY_SIZE_BYTES        8
#define DESC_SIZE_BYTES         6

#define ENTRY_PRESENT           (1 << 7)

// Access byte fields.
//
#define ACCESS_DPL_KERNEL       (0 << 5)
#define ACCESS_DPL_USER         (3 << 5)
#define ACCESS_S_FLAG           (1 << 4)
#define ACCESS_EXECUTABLE       (1 << 3)
#define ACCESS_CODE_READABLE    (1 << 1)
#define ACCESS_DATA_WRITABLE    (1 << 1)

// Other flags.
//
#define FLAG_LIMIT_IN_PAGES     (1 << 3)
#define FLAG_PROTECTED          (1 << 2)

static uint8_t gp_desc[6];
static uint8_t gpp_gdt[32][ENTRY_SIZE_BYTES];

static void fill_desc(uint8_t * p_desc, void const * p_gdt, uint16_t gdt_size);
static void fill_entry(uint8_t * p_entry, uint32_t base, uint32_t limit,
                       uint8_t access_byte, uint8_t flags);

void
gdt_init (void)
{
    fill_entry(gpp_gdt[1], 0, 0x000FFFFF,
               (ENTRY_PRESENT
                | ACCESS_DPL_KERNEL
                | ACCESS_S_FLAG
                | ACCESS_EXECUTABLE
                | ACCESS_CODE_READABLE),
               (FLAG_LIMIT_IN_PAGES | FLAG_PROTECTED));
    fill_entry(gpp_gdt[2], 0, 0x000FFFFF,
               (ENTRY_PRESENT
                | ACCESS_DPL_KERNEL
                | ACCESS_S_FLAG
                | ACCESS_DATA_WRITABLE),
               (FLAG_LIMIT_IN_PAGES | FLAG_PROTECTED));

    fill_desc(gp_desc, gpp_gdt, (sizeof(gpp_gdt) - 1));
    gdt_load(gp_desc);
}

static void
fill_desc (uint8_t * p_desc, void const * p_gdt, uint16_t gdt_size)
{
    __builtin_memset(p_desc, 0, DESC_SIZE_BYTES);

    __builtin_memcpy(&p_desc[0], &gdt_size, 2);
    __builtin_memcpy(&p_desc[2], &p_gdt, 4);
}

static void
fill_entry (uint8_t * p_entry, uint32_t base, uint32_t limit,
            uint8_t access_byte, uint8_t flags)
{
    __builtin_memset(p_entry, 0, ENTRY_SIZE_BYTES);

    p_entry[0] |= ((limit >>  0) & 0xFF); // limit[00..07]
    p_entry[1] |= ((limit >>  8) & 0xFF); // limit[08..15]
    p_entry[6] |= ((limit >> 16) & 0x0F); // limit[16..19]

    p_entry[2] |= ((base >>  0) & 0xFF);  // base[00..07]
    p_entry[3] |= ((base >>  8) & 0xFF);  // base[08..15]
    p_entry[4] |= ((base >> 16) & 0xFF);  // base[16..23]
    p_entry[7] |= ((base >> 24) & 0xFF);  // base[24..31]

    p_entry[5] |= access_byte;
    p_entry[6] |= ((flags & 0x0F) << 4);
}
