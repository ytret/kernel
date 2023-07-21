#include <kshell/cmd.h>
#include <mbi.h>
#include <printf.h>
#include <string.h>
#include <term.h>

static void cmd_clear(void);
static void cmd_mbimap(void);

void
kshell_cmd_parse (char const * p_cmd)
{
    if (string_equals(p_cmd, "clear"))
    {
        cmd_clear();
    }
    else if (string_equals(p_cmd, "mbimap"))
    {
        cmd_mbimap();
    }
    else
    {
        printf("kshell: unknown command: '%s'\n", p_cmd);
    }
}

static void
cmd_clear (void)
{
    term_clear();
}

static void
cmd_mbimap (void)
{
    mbi_t const * p_mbi = mbi_get_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MMAP))
    {
        printf("No memory map in MBI\n");
        return;
    }

    uint32_t byte = 0;
    while (byte < p_mbi->mmap_length)
    {
        uint32_t const * p_entry =
            ((uint32_t const * ) (p_mbi->mmap_addr + byte));

        uint32_t size      = *(p_entry + 0);
        uint64_t base_addr = *((uint64_t const *) (p_entry + 1));
        uint64_t length    = *((uint64_t const *) (p_entry + 3));
        uint32_t type      = *(p_entry + 5);

        uint64_t end = (base_addr + length);

        printf("size = %d, addr = 0x%P", size, ((uint32_t) base_addr));
        if (end >> 32)
        {
            printf(", end = 0x%08X%08X",
                   ((uint32_t) (end >> 32)), ((uint32_t) end));
        }
        else
        {
            printf(", end = 0x%08X", ((uint32_t) end));
        }
        printf(", type = %d\n", type);

        byte += (4 + size);
    }
}
