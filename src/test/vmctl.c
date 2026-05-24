#include "log.h"
#include "test/vmctl.h"
#include "vmctl_arch.h"

[[gnu::noreturn]] void vmctl_exit(int code) {
    LOG_FLOW("code %d", code);
    vmctl_arch_exit(code);
}
