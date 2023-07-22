/*
 * Command handling functions of kshell.
 *
 * Adding a new command:
 *
 *      1. Increment NUM_CMDS.
 *
 *      2. Add the command name string to gp_cmd_names[] at index N.
 *
 *      3. Add the static function declaration near the other command handlers.
 *
 *      4. Add the handler function pointer to p_cmd_funs[] at the same index N
 *         in kshell_cmd_parse().
 *
 *      5. Add the function definition near the other command handlers.
 */

#include <kshell/cmd.h>
#include <mbi.h>
#include <printf.h>
#include <string.h>
#include <term.h>

#define NUM_CMDS        4

static char const * const gp_cmd_names[NUM_CMDS] =
{
    "clear",
    "help",
    "mbimap",
    "mbimod",
};

static void cmd_clear(void);
static void cmd_help(void);
static void cmd_mbimap(void);
static void cmd_mbimod(void);

void
kshell_cmd_parse (char const * p_cmd)
{
    static void (* const p_cmd_funs[NUM_CMDS])(void) =
        {
            cmd_clear,
            cmd_help,
            cmd_mbimap,
            cmd_mbimod,
        };

    for (size_t idx = 0; idx < NUM_CMDS; idx++)
    {
        if (string_equals(p_cmd, gp_cmd_names[idx]))
        {
            p_cmd_funs[idx]();
            return;
        }
    }

    printf("kshell: unknown command: '%s'\n", p_cmd);
}

static void
cmd_clear (void)
{
    term_clear();
}

static void
cmd_help (void)
{
    printf("Available commands:\n");
    for (size_t idx = 0; idx < NUM_CMDS; idx++)
    {
        printf("%s", gp_cmd_names[idx]);
        if (idx != (NUM_CMDS - 1))
        {
            printf(", ");
        }
    }
    printf("\n");
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

static void
cmd_mbimod (void)
{
    mbi_t const * p_mbi = mbi_get_ptr();

    if (!(p_mbi->flags & (1 << 3)))
    {
        printf("No modules in MBI\n");
        return;
    }

    printf("Module count = %u\n", p_mbi->mods_count);

    for (size_t idx = 0; idx < p_mbi->mods_count; idx++)
    {
        mbi_mod_t const * p_mod = ((mbi_mod_t const *) p_mbi->mods_addr);

        printf("Module %d: ", idx);
        if (p_mod->string)
        {
            printf("'%s', ", p_mod->string);
        }
        else
        {
            printf("string = NULL, ");
        }
        printf("start = %P, end = %P, size = %u", p_mod->mod_start,
               p_mod->mod_end, (p_mod->mod_end - p_mod->mod_start));
        printf("\n");

        p_mod += 1;
    }
}
