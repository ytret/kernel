#include "fildes.h"
#include "kshell/kshcmd/kshcmd.h"
#include "kshell/kshell.h"
#include "kshell/kshinput.h"
#include "log.h"
#include "vfs/file.h"

void kshell(void) {
    LOG_INFO("enter kshell");

    file_err_t err;
    size_t fd_in;
    size_t fd_out;

    LOG_DEBUG("open /dev/tty0");

    err = fd_open("/dev/tty0", FILE_RDONLY, &fd_in);
    if (err != FILE_ERR_NONE) {
        LOG_ERROR("failed to open /dev/tty0 for reading, error %d", err);
        return;
    }

    err = fd_open("/dev/tty0", FILE_WRONLY, &fd_out);
    if (err != FILE_ERR_NONE) {
        LOG_ERROR("failed to open /dev/tty0 for writing, error %d", err);
        fd_close(fd_in);
        return;
    }

    kshinput_init(fd_in);

    for (;;) {
        fd_write(fd_out, "> ", 2, NULL);
        char const *p_cmd = kshinput_line();
        if (!p_cmd) { break; }
        kshcmd_parse(p_cmd);
    }

    fd_close(fd_out);
    fd_close(fd_in);
}
