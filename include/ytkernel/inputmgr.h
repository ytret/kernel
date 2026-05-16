#pragma once

#include "tty.h"

void inputmgr_init(void);
void inputmgr_set_tty(tty_t *tty);
size_t inputmgr_write(const void *buf, size_t buf_size);
