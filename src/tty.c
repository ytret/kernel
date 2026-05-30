#include <limits.h>

#include "assert.h"
#include "chardev.h"
#include "heap.h"
#include "kmutex.h"
#include "kprintf.h"
#include "log.h"
#include "memfun.h"
#include "tty.h"

#define TTY_INBUF_SIZE     128
#define TTY_OUTBUF_SIZE    256
#define TTY_NODE_NAME_SIZE 64

struct tty {
    bool is_inited;
    task_mutex_t lock;
    size_t lock_cnt;

    size_t unique_id;
    ldisc_t ldisc;
    chardev_t *out_dev;
    chardev_t chardev;
};

static tty_t g_tty_boot_tty;
static _Atomic size_t g_tty_next_id;

static void prv_tty_set_ldisc_type(tty_t *tty, ldisc_type_t new_type);

static int prv_tty_chardev_write(void *ctx, const void *buf, size_t buf_size);
static int prv_tty_chardev_read(void *ctx, void *buf, size_t buf_size);

static const chardev_ops_t g_tty_chardev_ops = {
    .f_write = prv_tty_chardev_write,
    .f_read = prv_tty_chardev_read,
};

[[gnu::artificial]]
static inline void prv_tty_assert_lock(tty_t *tty) {
    ASSERT(mutex_caller_owns(&tty->lock));
}

tty_t *tty_get_boot_tty(void) {
    return &g_tty_boot_tty;
}

void tty_init(tty_t *tty) {
    ASSERT(tty != NULL);

    kmemset(tty, 0, sizeof(*tty));
    mutex_init(&tty->lock);

    const size_t id = g_tty_next_id++;
    tty->unique_id = id;

    ldisc_cooked_init(&tty->ldisc);

    tty->chardev.ctx = tty;
    tty->chardev.type = CHARDEV_TTY;
    tty->chardev.ops = &g_tty_chardev_ops;

    tty->is_inited = true;
}

tty_t *tty_new(void) {
    tty_t *const tty = heap_alloc_aligned(sizeof(tty_t), _Alignof(tty_t));
    tty_init(tty);
    return tty;
}

vfs_err_t tty_mk_devfs_node(tty_t *tty, devfs_ctx_t *devfs) {
    // FIXME: check if it already has a device node?

    DEBUG_ASSERT(devfs != NULL);

    char name[TTY_NODE_NAME_SIZE];
    const int len = ksnprintf(name, sizeof(name), "tty%zu", tty->unique_id);
    if (len >= TTY_NODE_NAME_SIZE) {
        PANIC(
            "tty name would be too large (%d chars) to fit in %zu-byte buffer",
            len, sizeof(name));
    }

    return devfs_add_chardev(devfs, name, &tty->chardev);
}

size_t tty_get_id(tty_t *tty) {
    return tty->unique_id;
}

bool tty_is_inited(tty_t *tty) {
    return tty->is_inited;
}

void tty_lock(tty_t *tty) {
    if (!mutex_caller_owns(&tty->lock)) { mutex_acquire(&tty->lock); }
    tty->lock_cnt++;
}

void tty_unlock(tty_t *tty) {
    ASSERT(tty->lock_cnt > 0);
    tty->lock_cnt--;
    if (tty->lock_cnt == 0) { mutex_release(&tty->lock); }
}

void tty_set_out(tty_t *tty, chardev_t *chardev) {
    DEBUG_ASSERT(tty != NULL);
    tty->out_dev = chardev;
    ldisc_cooked_set_echo_dev(&tty->ldisc, chardev);
}

bool tty_set_ldisc_type(tty_t *tty, ldisc_type_t ldisc_type) {
    DEBUG_ASSERT(tty != NULL);
    ASSERT(ldisc_type != LDISC_UNINIT);

    tty_lock(tty);
    const bool changed =
        ((ldisc_type == LDISC_RAW && tty->ldisc.type != LDISC_RAW) ||
         (ldisc_type == LDISC_COOKED && tty->ldisc.type != LDISC_COOKED));
    if (changed) { prv_tty_set_ldisc_type(tty, ldisc_type); }
    tty_unlock(tty);

    return changed;
}

ldisc_type_t tty_get_ldisc_type(tty_t *tty) {
    DEBUG_ASSERT(tty != NULL);
    return tty->ldisc.type == LDISC_RAW ? LDISC_RAW : LDISC_COOKED;
}

size_t tty_write_input(tty_t *tty, const void *buf, size_t buf_size) {
    DEBUG_ASSERT(tty != NULL);
    prv_tty_assert_lock(tty);

    if (!tty->ldisc.ctx) {
        LOG_ERROR("tty %p has no line discipline", tty);
        return 0;
    }

    ASSERT(tty->ldisc.ops != NULL);
    ASSERT(tty->ldisc.ops->f_write != NULL);
    return tty->ldisc.ops->f_write(tty->ldisc.ctx, buf, buf_size);
}

size_t tty_read_input(tty_t *tty, void *buf, size_t buf_size) {
    DEBUG_ASSERT(tty != NULL);
    DEBUG_ASSERT(buf != NULL);

    tty_lock(tty);

    if (!tty->ldisc.ctx) {
        LOG_ERROR("tty %p has no line discipline", tty);
        tty_unlock(tty);
        return 0;
    }

    ASSERT(tty->ldisc.ops != NULL);
    ASSERT(tty->ldisc.ops->f_read != NULL);

    const ldisc_op_read_t ldisc_op_read = tty->ldisc.ops->f_read;
    void *const ldisc_ctx = tty->ldisc.ctx;

    tty_unlock(tty);

    return ldisc_op_read(ldisc_ctx, buf, buf_size);
}

size_t tty_write_output(tty_t *tty, const void *buf, size_t buf_size) {
    DEBUG_ASSERT(tty != NULL);
    DEBUG_ASSERT(buf != NULL);

    if (!tty->out_dev) { return 0; }

    ASSERT(tty->out_dev->ctx != NULL);
    ASSERT(tty->out_dev->ops != NULL);
    return tty->out_dev->ops->f_write(tty->out_dev->ctx, buf, buf_size);
}

static void prv_tty_set_ldisc_type(tty_t *tty, ldisc_type_t new_type) {
    ldisc_destroy(&tty->ldisc);

    bool type_ok = false;
    switch (new_type) {
    case LDISC_UNINIT:
        PANIC("called with new_type equal to LDISC_UNINIT");
        break;
    case LDISC_RAW:
        type_ok = true;
        ldisc_raw_init(&tty->ldisc);
        break;
    case LDISC_COOKED:
        type_ok = true;
        ldisc_cooked_init(&tty->ldisc);
        if (tty->out_dev) {
            ldisc_cooked_set_echo_dev(&tty->ldisc, tty->out_dev);
        }
        break;
    }
    ASSERT(type_ok);
}

static int prv_tty_chardev_write(void *v_ctx, const void *buf,
                                 size_t buf_size) {
    DEBUG_ASSERT(v_ctx != NULL);

    tty_t *const tty = v_ctx;
    ASSERT(tty->is_inited);

    ASSERT(tty->out_dev != NULL);
    ASSERT(tty->out_dev->ops != NULL);
    ASSERT(tty->out_dev->ops->f_write != NULL);
    return tty->out_dev->ops->f_write(tty->out_dev->ctx, buf, buf_size);
}

static int prv_tty_chardev_read(void *v_ctx, void *buf, size_t buf_size) {
    DEBUG_ASSERT(v_ctx != NULL);

    tty_t *const tty = v_ctx;
    ASSERT(tty->is_inited);

    ldisc_t *const ldisc = &tty->ldisc;
    ASSERT(ldisc->ops != NULL);
    ASSERT(ldisc->ops->f_read != NULL);
    const size_t num_read = ldisc->ops->f_read(ldisc->ctx, buf, buf_size);
    // FIXME: return ssize_t or size_t
    ASSERT(num_read <= INT_MAX);
    return (int)num_read;
}
