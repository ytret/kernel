#pragma once

#include <stddef.h>

void kshinput_init(size_t fd_in);

/**
 * Synchronously reads a line from the boot TTY and returns it.
 *
 * @warning
 * Each consecutive call invalidates the pointer returned by the previous call.
 */
const char *kshinput_line(void);
