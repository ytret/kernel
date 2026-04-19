#include <setjmp.h>
#include <ytkernel/panic.h>

int setjmp(jmp_buf env) {
    PANIC("stub %s called", __func__);
}

[[noreturn]] void longjmp(jmp_buf env, int val) {
    PANIC("stub %s called", __func__);
}
