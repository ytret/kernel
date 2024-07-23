#include "kprintf.h"
#include "syscall.h"

void syscall_dispatch(void) {
    kprintf("Syscall\n");
}
