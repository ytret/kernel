#include <stddef.h>

#include "fildes.h"
#include "kshell/kshinput.h"
#include "log.h"

#define CMD_BUF_SIZE 256

static size_t g_kshinput_fd;
static char g_kshinput_buf[CMD_BUF_SIZE];

void kshinput_init(size_t fd_in) {
    g_kshinput_fd = fd_in;
}

const char *kshinput_line(void) {
    size_t num_read;
    const file_err_t err =
        fd_read(g_kshinput_fd, g_kshinput_buf, CMD_BUF_SIZE, &num_read);
    if (err != FILE_ERR_NONE) {
        LOG_ERROR("failed to read from fd %zu, error %d", g_kshinput_fd, err);
        return NULL;
    }

    if (num_read == 0) {
        g_kshinput_buf[0] = '\0';
    } else {
        g_kshinput_buf[num_read - 1] = '\0';
    }
    return g_kshinput_buf;
}
