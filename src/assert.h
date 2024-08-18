#pragma once

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) { panic_silent(); }                                       \
    } while (0);
