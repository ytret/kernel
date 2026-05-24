#pragma once

#define ARCH_VEC_HALT          0xF1 //!< Halt on panic.
#define ARCH_VEC_TLB_SHOOTDOWN 0xF2 //!< TLB shootdown.

// physical/virtual (identity-mapped)
#define ARCH_AP_TRAMPOLINE_ADDR 0x8000
#define ARCH_AP_TRAMPLINE_ARGS  0x8800

// virtual
#define ARCH_AP_INIT_STACK_TOP 0xA000 // page 0x9000..0x9FFF
