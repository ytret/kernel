#include "kprintf.h"
#include "kshell/kshcmd/kshcmd.h"
#include "kshell/kshell.h"
#include "kshell/kshinput.h"

void kshell(void) {
    for (;;) {
        kprintf("> ");
        char const *p_cmd = kshinput_line();
        kshcmd_parse(p_cmd);
    }
}
