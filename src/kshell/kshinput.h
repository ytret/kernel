#pragma once

void kshinput_init(void);

/**
 * Synchronously reads a line from the boot TTY and returns it.
 *
 * @warning
 * Each consecutive call invalidates the pointer returned by the previous call.
 */
const char *kshinput_line(void);
