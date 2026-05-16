/*
 * Ring buffer without data overwrite.
 */

#include <string.h>

#include "ringbuf.h"

static size_t count_bytes_wrapped(ringbuf_t *p_ringbuf, uint8_t *p_read_write,
                                  size_t num_bytes);

/*
 * Initializes the ringbuf_t struct fields.
 *
 * Arguments:
 *   p_ringbuf - pointer to the ringbuf_t struct
 *   p_start - pointer to the start of the underlying buffer
 *   size - size of the underlying buffer
 */
void ringbuf_init(ringbuf_t *p_ringbuf, void *p_start, size_t size) {
    p_ringbuf->p_start = p_start;
    p_ringbuf->p_read = p_start;
    p_ringbuf->p_write = p_start;

    p_ringbuf->size = size;
    p_ringbuf->bytes_used = 0;
}

/*
 * Reads bytes from the ring buffer.
 *
 * If the ring buffer has less readable bytes than `num_bytes`, then reads only
 * that amount of bytes.
 *
 * Arguments:
 *   p_ringbuf - pointer to the ringbuf_t struct
 *   p_buffer - pointer to the destination buffer
 *   num_bytes - maximum number of bytes to read into the destination buffer
 *
 * Returns the number of bytes read from the ring buffer and written to
 * `p_buffer`.
 */
size_t ringbuf_read(ringbuf_t *p_ringbuf, void *p_buffer, size_t num_bytes) {
    // If the ringbuf has less bytes than required, then read all that is left.
    size_t bytes_used = ringbuf_bytes_used(p_ringbuf);
    size_t bytes_read = num_bytes > bytes_used ? bytes_used : num_bytes;

    // Count how many bytes fit until the end of the underlying buffer and how
    // many are wrapped.
    size_t bytes_wrapped =
        count_bytes_wrapped(p_ringbuf, p_ringbuf->p_read, bytes_read);
    size_t bytes_until_end = bytes_read - bytes_wrapped;

    // Copy data to `p_buffer`.
    __builtin_memcpy(p_buffer, p_ringbuf->p_read, bytes_until_end);
    __builtin_memcpy(((uint8_t *)p_buffer) + bytes_until_end,
                     p_ringbuf->p_start, bytes_wrapped);

    // Update the read pointer.
    if (0 == bytes_wrapped) {
        p_ringbuf->p_read += bytes_read;
    } else {
        p_ringbuf->p_read = p_ringbuf->p_start + bytes_wrapped;
    }

    p_ringbuf->bytes_used -= bytes_read;

    return bytes_read;
}

/*
 * Writes bytes to the ring buffer.
 *
 * If the free space in the ring buffer is not sufficient to fit `num_bytes`
 * bytes, then writes as much bytes as there is free space.
 *
 * Arguments:
 *   p_ringbuf - pointer to the ringbuf_t struct
 *   p_buffer - pointer to the source buffer
 *   num_bytes - maximum number of bytes to write into the ring buffer
 *
 * Returns the number of bytes written to the ring buffer.
 */
size_t ringbuf_write(ringbuf_t *p_ringbuf, void const *p_buffer,
                     size_t num_bytes) {
    size_t bytes_free = p_ringbuf->size - ringbuf_bytes_used(p_ringbuf);
    size_t bytes_written = num_bytes > bytes_free ? bytes_free : num_bytes;

    size_t bytes_wrapped =
        count_bytes_wrapped(p_ringbuf, p_ringbuf->p_write, bytes_written);
    size_t bytes_until_end = bytes_written - bytes_wrapped;

    __builtin_memcpy(p_ringbuf->p_write, p_buffer, bytes_until_end);
    __builtin_memcpy(p_ringbuf->p_start,
                     ((uint8_t *)p_buffer) + bytes_until_end, bytes_wrapped);

    if (0 == bytes_wrapped) {
        p_ringbuf->p_write += bytes_written;
    } else {
        p_ringbuf->p_write = p_ringbuf->p_start + bytes_wrapped;
    }

    p_ringbuf->bytes_used += bytes_written;

    return bytes_written;
}

/*
 * Advances the write pointer to an offset from the start of the underlying
 * buffer. Updates the number of used bytes.
 *
 * Arguments:
 *   p_ringbuf - pointer to the ringbuf_t struct
 *   offset_from_start - offset from the start of the underlying buffer in bytes
 */
void ringbuf_update_write_ptr(ringbuf_t *p_ringbuf, size_t offset_from_start) {
    size_t bytes_readable_until_end;

    p_ringbuf->p_write = p_ringbuf->p_start + offset_from_start;

    // Recalculate the number of used bytes.
    if (p_ringbuf->p_write >= p_ringbuf->p_read) {
        p_ringbuf->bytes_used = p_ringbuf->p_write - p_ringbuf->p_read;
    } else {
        bytes_readable_until_end =
            (p_ringbuf->p_start + p_ringbuf->size) - p_ringbuf->p_read;
        p_ringbuf->bytes_used = (p_ringbuf->p_write - p_ringbuf->p_start) +
                                bytes_readable_until_end;
    }
}

/*
 * Returns the number of used bytes.
 */
size_t ringbuf_bytes_used(ringbuf_t *p_ringbuf) {
    return p_ringbuf->bytes_used;
}

/*
 * Returns `true` if the ring buffer is empty.
 */
bool ringbuf_is_empty(ringbuf_t *p_ringbuf) {
    return p_ringbuf->bytes_used == 0;
}

/*
 * Counts the number of bytes that will appear at the start of the underlying
 * buffer after a read or write due to wrapping.
 *
 * Arguments:
 *   p_ringbuf - pointer to the ringbuf_t struct
 *   p_read_write - either `p_ringbuf->p_read` or `p_ringbuf->p_write`
 *   num_bytes - number of bytes for the suppposed operation
 */
static size_t count_bytes_wrapped(ringbuf_t *p_ringbuf, uint8_t *p_read_write,
                                  size_t num_bytes) {
    size_t current_pos =
        ((uintptr_t)p_read_write) - ((uintptr_t)p_ringbuf->p_start);
    size_t bytes_until_end = p_ringbuf->size - current_pos;

    if (bytes_until_end < num_bytes) {
        return num_bytes - bytes_until_end;
    } else {
        return 0;
    }
}
