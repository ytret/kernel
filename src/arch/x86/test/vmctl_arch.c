#include "panic.h"
#include "test/vmctl_arch.h"

#include "arch/x86/port.h"

#define VMCTL_EXIT_PORT 0xF4

[[gnu::noreturn]] void vmctl_arch_exit(int code) {
    port_outb(VMCTL_EXIT_PORT, code);
    PANIC("reached end of %s", __func__);
}
