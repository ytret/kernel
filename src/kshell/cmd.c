#include <kshell/cmd.h>
#include <printf.h>
#include <string.h>
#include <term.h>

static void cmd_clear(void);

void
kshell_cmd_parse (char const * p_cmd)
{
    if (string_equals(p_cmd, "clear"))
    {
        cmd_clear();
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
