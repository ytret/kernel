#include "log.h"
#include "panic.h"
#include "test/vmctl.h"

#include "arch/x86/port.h"

#define VMCTL_EXIT_PORT 0xF4

[[gnu::noreturn]] void vmctl_exit(int code) {
    LOG_FLOW("code %d", code);

    port_outb(VMCTL_EXIT_PORT, code);

    PANIC("reached end of %s", __func__);
}
