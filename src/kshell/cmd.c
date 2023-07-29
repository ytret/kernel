/*
 * Command handling functions of kshell.
 *
 * To add a new command:
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

#include <alloc.h>
#include <elf.h>
#include <kshell/cmd.h>
#include <kshell/vasview.h>
#include <mbi.h>
#include <panic.h>
#include <pci.h>
#include <printf.h>
#include <string.h>
#include <taskmgr.h>
#include <term.h>
#include <vmm.h>

#include <cpuid.h>

#define NUM_CMDS        10
#define MAX_ARGS        32

static char const * const gp_cmd_names[NUM_CMDS] =
{
    "clear",
    "help",
    "mbimap",
    "mbimod",
    "elfhdr",
    "exec",
    "tasks",
    "vasview",
    "cpuid",
    "pci",
};

static uint32_t g_exec_entry;

static void cmd_clear(char ** pp_args, size_t num_args);
static void cmd_help(char ** pp_args, size_t num_args);
static void cmd_mbimap(char ** pp_args, size_t num_args);
static void cmd_mbimod(char ** pp_args, size_t num_args);
static void cmd_elfhdr(char ** pp_args, size_t num_args);
static void cmd_exec(char ** pp_args, size_t num_args);
static void cmd_exec_entry(void);
static void cmd_tasks(char ** pp_args, size_t num_args);
static void cmd_vasview(char ** pp_args, size_t num_args);
static void cmd_cpuid(char ** pp_args, size_t num_args);
static void cmd_pci(char ** pp_args, size_t num_args);

void
kshell_cmd_parse (char const * p_cmd)
{
    char * pp_args[MAX_ARGS];
    size_t num_args = string_split(p_cmd, ' ', true, pp_args, MAX_ARGS);

    if (0 == num_args)
    {
        return;
    }

    if (num_args > MAX_ARGS)
    {
        printf("kshell: too much arguments (more than %u)\n", MAX_ARGS);
        return;
    }

    static void (* const p_cmd_funs[NUM_CMDS])(char ** pp_args,
                                               size_t num_args) =
        {
            cmd_clear,
            cmd_help,
            cmd_mbimap,
            cmd_mbimod,
            cmd_elfhdr,
            cmd_exec,
            cmd_tasks,
            cmd_vasview,
            cmd_cpuid,
            cmd_pci,
        };

    for (size_t idx = 0; idx < NUM_CMDS; idx++)
    {
        if (string_equals(pp_args[0], gp_cmd_names[idx]))
        {
            p_cmd_funs[idx](pp_args, num_args);
            return;
        }
    }

    printf("kshell: unknown command: '%s'\n", pp_args[0]);
}

static void
cmd_clear (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

    term_clear();
}

static void
cmd_help (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

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
cmd_mbimap (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

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
cmd_mbimod (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const * p_mbi = mbi_get_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MODS))
    {
        printf("No modules in MBI\n");
        return;
    }

    printf("Module count = %u\n", p_mbi->mods_count);

    mbi_mod_t const * p_mod = ((mbi_mod_t const *) p_mbi->mods_addr);
    for (size_t idx = 0; idx < p_mbi->mods_count; idx++)
    {
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

static void
cmd_elfhdr (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const * p_mbi = mbi_get_ptr();
    void const * p_elf_user = mbi_find_mod(p_mbi, "user");
    if (!p_elf_user)
    {
        printf("Module 'user' is not found\n");
        return;
    }

    elf_dump(p_elf_user);
}

static void
cmd_exec (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

    mbi_t const * p_mbi = mbi_get_ptr();

    if (!(p_mbi->flags & MBI_FLAG_MODS))
    {
        printf("Module 'user' is not found\n");
        return;
    }

    void const * p_elf_user = NULL;
    for (size_t idx = 0; idx < p_mbi->mods_count; idx++)
    {
        mbi_mod_t const * p_mods = ((mbi_mod_t const *) p_mbi->mods_addr);

        if (!p_mods[idx].string)
        {
            continue;
        }

        if (string_equals(((char const *) p_mods[idx].string), "user"))
        {
            p_elf_user = ((void const *) p_mods[idx].mod_start);
        }
    }

    if (!p_elf_user)
    {
        printf("Module 'user' is not found\n");
        return;
    }

    uint32_t * p_dir = vmm_clone_kvas();
    printf("kshell: cloned kernel VAS at %P\n", p_dir);

    bool ok = elf_load(p_dir, p_elf_user, &g_exec_entry);
    if (!ok)
    {
        printf("kshell: failed to load the executable\n");
    }

    printf("kshell: successfully loaded\n");
    printf("kshell: entry at 0x%08x\n", g_exec_entry);

    taskmgr_new_user_task(p_dir, ((uint32_t) cmd_exec_entry));

    alloc_free(p_dir);
}

static void
cmd_exec_entry (void)
{
    // taskmgr_switch_tasks() requires that task entries enable interrupts.
    //
    __asm__ volatile ("sti");

    taskmgr_go_usermode(g_exec_entry);

    printf("kshell: call to taskmgr_go_usermode() has returned\n");
    panic("unexpected behavior");
}

static void
cmd_tasks (char ** pp_args, size_t num_args)
{
    if (1 != num_args)
    {
        printf("Usage: %s\n", pp_args[0]);
        return;
    }

    taskmgr_dump_tasks();
}

static void
cmd_vasview (char ** pp_args, size_t num_args)
{
    if (num_args > 2)
    {
        printf("Usage: %s [pgdir]\n", pp_args[0]);
        printf("Optional arguments:\n");
        printf("  pgdir  virtual address of a page directory to view"
               " (default: kshell task VAS)\n");
        return;
    }

    uint32_t pgdir;
    __asm__ volatile ("mov %%cr3, %0"
                      : "=a" (pgdir)
                      : /* no inputs */
                      : /* no clobber */);

    if (2 == num_args)
    {
        bool ok = string_to_uint32(pp_args[1], &pgdir, 16);
        if (!ok)
        {
            printf("Invalid argument: '%s'\n", pp_args[1]);
            printf("pgdir must be a hexadecimal 32-bit unsigned integer\n");
            return;
        }
    }

    vasview(pgdir);
}

static void
cmd_cpuid (char ** pp_args, size_t num_args)
{
    if (2 != num_args)
    {
        printf("Usage: %s <leaf>\n", pp_args[0]);
        return;
    }

    uint32_t leaf;
    bool b_arg_ok = string_to_uint32(pp_args[1], &leaf, 10);
    if (!b_arg_ok)
    {
        printf("Invalid argument: '%s'\n", pp_args[1]);
        printf("leaf must be a decimal 32-bit unsigned integer\n");
        return;
    }

    unsigned int eax, ebx, ecx, edx;
    int cpuid_ok = __get_cpuid(leaf, &eax, &ebx, &ecx, &edx);
    if (!cpuid_ok)
    {
        printf("Unsupported cpuid leaf: %u\n", leaf);
        return;
    }

    printf("EAX = %08x\n", eax);
    printf("EBX = %08x\n", ebx);
    printf("ECX = %08x\n", ecx);
    printf("EDX = %08x\n", edx);
}

static void
cmd_pci (char ** pp_args, size_t num_args)
{
    if (num_args < 2)
    {
        printf("Usage: %s <cmd> [args]\n", pp_args[0]);
        printf("cmd must be one of: dump, walk\n");
        printf("args depend on cmd\n");
        return;
    }

    if (string_equals(pp_args[1], "walk"))
    {
        if (num_args != 2)
        {
            printf("Usage: pci walk\n");
            return;
        }

        pci_walk();
    }
    else if (string_equals(pp_args[1], "dump"))
    {
        if (num_args != 3)
        {
            printf("Usage: pci dump <device number>\n");
            return;
        }

        uint32_t dev;
        bool b_ok = string_to_uint32(pp_args[2], &dev, 10);
        if ((!b_ok) || (dev >= 32))
        {
            printf("Invalid argument: '%s'\n", pp_args[2]);
            printf("device number must be an unsigned decimal number less than"
                   " 32\n");
            return;
        }

        pci_dump_config(0, dev);
    }
    else
    {
        printf("pci: unknown command: '%s'\n", pp_args[1]);
        return;
    }
}
