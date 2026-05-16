#include "assert.h"
#include "inputmgr.h"
#include "log.h"

typedef struct {
    // FIXME: does this need lock?
    tty_t *active_tty;
} inputmgr_t;

static inputmgr_t g_inputmgr;

void inputmgr_init(void) {
}

void inputmgr_set_tty(tty_t *tty) {
    g_inputmgr.active_tty = tty;
}

size_t inputmgr_write(const void *buf, size_t buf_size) {
    DEBUG_ASSERT(buf != NULL);

    tty_t *const tty = g_inputmgr.active_tty;
    if (!tty) {
        LOG_ERROR("no active tty");
        return 0;
    }

    tty_lock(tty);
    const size_t num_written = tty_write_input(tty, buf, buf_size);
    tty_unlock(tty);
    return num_written;
}
