#pragma once

#include <stdlib.h>

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) { abort(); }                                              \
    } while (0);
