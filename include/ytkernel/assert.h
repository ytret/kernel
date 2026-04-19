#pragma once

#include "panic.h" // IWYU pragma: keep

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) { PANIC("%s", "assertion failed: " #cond); }              \
    } while (0);
