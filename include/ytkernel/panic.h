#pragma once

#include <stdint.h>

#define PANIC(...) panic_helper(__FILE_NAME__, __func__, __LINE__, __VA_ARGS__)
#define PANIC_ST(ST, ...)                                                      \
    panic_helper_st(__FILE_NAME__, __func__, __LINE__, (ST), __VA_ARGS__)

typedef struct {
    int init_ebp;
    int init_eip;
} panic_traceinit_t;

[[gnu::noreturn]] void panic_helper(const char *file, const char *func,
                                    int line, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
[[gnu::noreturn]] void
panic_helper_st(const char *file, const char *func, int line,
                const panic_traceinit_t *stacktrace, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

[[gnu::noreturn]] void panic_nomsg(void);
[[gnu::noreturn]] void panic_nested(void);

int panic_init_lua(void *v_L);

extern int panic_walk_stack(uint32_t *arr_addr, uint32_t max_items,
                            uint32_t init_ebp);
