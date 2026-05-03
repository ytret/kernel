#pragma once

#include "panic.h" // IWYU pragma: keep

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) { PANIC("%s", "assertion failed: " #cond); }              \
    } while (0);

#ifdef YTKERNEL_DEBUG
#define DEBUG_ASSERT(cond) ASSERT(cond)
#else
#define DEBUG_ASSERT(cond) (void)(cond)
#endif
