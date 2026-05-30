/**
 * @file tty.h
 * TTY layer API.
 *
 * A TTY object is responsible for handling user/task input using a *line
 * discipline*.
 */

#pragma once

#include <stddef.h>

#include "chardev.h"
#include "ldisc.h"

typedef struct tty tty_t;

tty_t *tty_get_boot_tty(void);

void tty_init(tty_t *tty);
tty_t *tty_new(void);

size_t tty_get_id(tty_t *tty);

bool tty_is_inited(tty_t *tty);
void tty_lock(tty_t *tty);
void tty_unlock(tty_t *tty);

void tty_set_out(tty_t *tty, chardev_t *chardev);

bool tty_set_ldisc_type(tty_t *tty, ldisc_type_t ldisc_type);
ldisc_type_t tty_get_ldisc_type(tty_t *tty);

size_t tty_write_input(tty_t *tty, const void *buf, size_t buf_size);
size_t tty_read_input(tty_t *tty, void *buf, size_t buf_size);

size_t tty_write_output(tty_t *tty, const void *buf, size_t buf_size);
