#include "assert.h"
#include "inputmgr.h"
#include "kspinlock.h"
#include "log.h"

typedef struct {
    spinlock_t lock;
    tty_t *active_tty;
} inputmgr_t;

static inputmgr_t g_inputmgr;

void inputmgr_init(void) {
    spinlock_init(&g_inputmgr.lock);
}

void inputmgr_set_tty(tty_t *tty) {
    spinlock_acquire(&g_inputmgr.lock);
    g_inputmgr.active_tty = tty;
    spinlock_release(&g_inputmgr.lock);
}

kerr_t inputmgr_write(const void *buf, size_t buf_size, size_t *out_written) {
    DEBUG_ASSERT(buf != NULL);

    spinlock_acquire(&g_inputmgr.lock);
    tty_t *const tty = g_inputmgr.active_tty;
    spinlock_release(&g_inputmgr.lock);
    if (!tty) {
        LOG_ERROR("no active tty");
        return 0;
    }

    tty_lock(tty);
    const kerr_t err = tty_write_input(tty, buf, buf_size, out_written);
    tty_unlock(tty);
    return err;
}
