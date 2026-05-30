/**
 * @file conmgr.h
 * Console manager layer.
 *
 * A console manager is a unique kernel object that creates consoles (in
 * addition to the boot console) and allows the kernel to switch between them.
 */

#pragma once

#include <stddef.h>

#include "fs/devfs.h"

void conmgr_init(size_t num_add_cons);
bool conmgr_switch(size_t idx);

bool conmgr_mk_tty_nodes(devfs_ctx_t *devfs);
