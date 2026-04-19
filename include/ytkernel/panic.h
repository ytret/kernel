#pragma once

#include <stdint.h>

#define PANIC(...) panic_helper(__FILE_NAME__, __func__, __LINE__, __VA_ARGS__)

[[gnu::noreturn]] void panic_helper(const char *file, const char *func,
                                    int line, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

[[gnu::noreturn]] void panic_nomsg(void);
[[gnu::noreturn]] void panic_nested(void);

extern int panic_walk_stack(uint32_t *arr_addr, uint32_t max_items);
