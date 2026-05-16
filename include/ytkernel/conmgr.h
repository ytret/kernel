#pragma once

#include <stddef.h>

void conmgr_init(size_t num_add_cons);
bool conmgr_switch(size_t idx);

[[gnu::noreturn]] void conmgr_task(void);
