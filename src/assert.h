#pragma once

#include "panic.h" // IWYU pragma: keep

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) { panic_silent(); }                                       \
    } while (0);
