#include <limits.h>

#include "assert.h"
#include "heap.h"
#include "ksemaphore.h"
#include "ldisc.h"
#include "memfun.h"
#include "ringbuf.h"

#define LDISC_RAW_BUF_SIZE  128
#define LDISC_RAW_BUF_ALIGN 8

#define LDISC_COOKED_LINE_SIZE  256
#define LDISC_COOKED_RING_SIZE  512
#define LDISC_COOKED_RING_ALIGN 8

typedef struct {
    char buf[LDISC_RAW_BUF_SIZE];
    size_t buf_size;

    ringbuf_t ringbuf;
    semaphore_t buf_sem;
} ldisc_raw_t;

typedef struct {
    char line_buf[LDISC_COOKED_LINE_SIZE];
    size_t line_len;

    void *ring_buf;
    ringbuf_t ringbuf;
    semaphore_t buf_sem;

    chardev_t *echo_dev;
} ldisc_cooked_t;

static size_t prv_ldisc_raw_write(void *ctx, const void *buf, size_t buf_size);
static size_t prv_ldisc_raw_read(void *ctx, void *buf, size_t buf_size);

static const ldisc_ops_t g_ldisc_raw_ops = {
    .f_write = prv_ldisc_raw_write,
    .f_read = prv_ldisc_raw_read,
};

static size_t prv_ldisc_cooked_write(void *ctx, const void *buf,
                                     size_t buf_size);
static size_t prv_ldisc_cooked_read(void *ctx, void *buf, size_t buf_size);

static const ldisc_ops_t g_ldisc_cooked_ops = {
    .f_write = prv_ldisc_cooked_write,
    .f_read = prv_ldisc_cooked_read,
};

void ldisc_raw_init(ldisc_t *ldisc) {
    ASSERT(ldisc != NULL);

    ldisc_raw_t *const raw =
        heap_alloc_aligned(sizeof(ldisc_raw_t), _Alignof(ldisc_raw_t));
    kmemset(raw, 0, sizeof(*raw));

    raw->buf_size = LDISC_RAW_BUF_SIZE;
    kmemset(raw->buf, 0, raw->buf_size);
    ringbuf_init(&raw->ringbuf, raw->buf, raw->buf_size);
    semaphore_init(&raw->buf_sem);

    ldisc->ctx = raw;
    ldisc->ops = &g_ldisc_raw_ops;
}

void ldisc_cooked_init(ldisc_t *ldisc) {
    ASSERT(ldisc != NULL);

    ldisc_cooked_t *const cooked =
        heap_alloc_aligned(sizeof(ldisc_cooked_t), _Alignof(ldisc_cooked_t));
    kmemset(cooked, 0, sizeof(*cooked));

    cooked->ring_buf =
        heap_alloc_aligned(LDISC_COOKED_RING_SIZE, LDISC_COOKED_RING_ALIGN);
    kmemset(cooked->ring_buf, 0, LDISC_COOKED_RING_SIZE);
    ringbuf_init(&cooked->ringbuf, cooked->ring_buf, LDISC_COOKED_RING_SIZE);
    semaphore_init(&cooked->buf_sem);

    ldisc->ctx = cooked;
    ldisc->ops = &g_ldisc_cooked_ops;
}

void ldisc_cooked_set_echo_dev(ldisc_t *ldisc, chardev_t *dev) {
    ASSERT(ldisc != NULL);
    ASSERT(ldisc->ctx != NULL);

    ldisc_cooked_t *const cooked = ldisc->ctx;
    cooked->echo_dev = dev;
}

static size_t prv_ldisc_raw_write(void *v_ctx, const void *buf,
                                  size_t buf_size) {
    ldisc_raw_t *const ctx = v_ctx;

    const size_t num_written = ringbuf_write(&ctx->ringbuf, buf, buf_size);
    ASSERT(num_written <= INT_MAX); // semaphore is int

    // FIXME: allow increasing by N at a time
    for (size_t i = 0; i < num_written; i++) {
        semaphore_increase(&ctx->buf_sem);
    }

    return num_written;
}

static size_t prv_ldisc_raw_read(void *v_ctx, void *buf, size_t buf_size) {
    ldisc_raw_t *const ctx = v_ctx;

    // FIXME: allow decreasing by N at a time
    for (size_t i = 0; i < buf_size; i++) {
        semaphore_decrease(&ctx->buf_sem);
    }

    return ringbuf_read(&ctx->ringbuf, buf, buf_size);
}

static void prv_ldisc_cooked_do_echo(ldisc_cooked_t *ctx, const void *buf,
                                     size_t len) {
    if (ctx->echo_dev && len > 0) {
        ctx->echo_dev->ops->f_write(ctx->echo_dev->ctx, buf, len);
    }
}

static size_t prv_ldisc_cooked_write(void *v_ctx, const void *buf,
                                     size_t buf_size) {
    ldisc_cooked_t *const ctx = v_ctx;
    const char *const bytes = buf;

    for (size_t i = 0; i < buf_size; i++) {
        const char ch = bytes[i];

        switch (ch) {
        case '\r':
        case '\n': {
            if (ctx->line_len < LDISC_COOKED_LINE_SIZE) {
                ctx->line_buf[ctx->line_len++] = '\n';
            }

            prv_ldisc_cooked_do_echo(ctx, "\n", 1);

            const size_t num_written =
                ringbuf_write(&ctx->ringbuf, ctx->line_buf, ctx->line_len);
            for (size_t j = 0; j < num_written; j++) {
                semaphore_increase(&ctx->buf_sem);
            }

            ctx->line_len = 0;
            break;
        }

        case '\b':
        case 0x7f: {
            if (ctx->line_len > 0) {
                ctx->line_len--;
                prv_ldisc_cooked_do_echo(ctx, "\b", 1);
            }
            break;
        }

        default: {
            if (ctx->line_len < LDISC_COOKED_LINE_SIZE) {
                ctx->line_buf[ctx->line_len++] = ch;
            }

            prv_ldisc_cooked_do_echo(ctx, &ch, 1);
            break;
        }
        }
    }

    return buf_size;
}

static size_t prv_ldisc_cooked_read(void *v_ctx, void *buf, size_t buf_size) {
    ldisc_cooked_t *const ctx = v_ctx;

    semaphore_decrease(&ctx->buf_sem);

    const size_t num_read = ringbuf_read(&ctx->ringbuf, buf, buf_size);

    for (size_t i = 1; i < num_read; i++) {
        semaphore_decrease(&ctx->buf_sem);
    }

    return num_read;
}
