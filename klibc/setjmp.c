#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#include <setjmp.h>
#include <ytkernel/log.h>

int setjmp(jmp_buf env) {
    LOG_FLOW("env %p", (void *)env);
    const int ret = __builtin_setjmp(env);
    LOG_FLOW("return %d", ret);
    return ret;
}

[[noreturn]] void longjmp(jmp_buf env, int val) {
    LOG_FLOW("env %p val %d", (void *)env, val);
    __builtin_longjmp(env, 1);
}
