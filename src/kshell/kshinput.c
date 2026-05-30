#include <stddef.h>

#include "kshell/kshinput.h"
#include "tty.h"

#define CMD_BUF_SIZE 256

static tty_t *g_kshinput_tty;
static char g_kshinput_buf[CMD_BUF_SIZE];

void kshinput_init(void) {
    g_kshinput_tty = tty_get_boot_tty();
}

const char *kshinput_line(void) {
    size_t num_read;
    const kerr_t err =
        tty_read_input(g_kshinput_tty, g_kshinput_buf, CMD_BUF_SIZE, &num_read);
    if (err != KERR_NONE || num_read == 0) {
        g_kshinput_buf[0] = '\0';
    } else {
        g_kshinput_buf[num_read - 1] = '\0';
    }
    return g_kshinput_buf;
}
