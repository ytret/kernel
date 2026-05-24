#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef YTKERNEL_ARCH_X86
#include "arch/x86/arch_types.h" // IWYU pragma: export
#else
#error "Please update include/ytkernel/arch.h to include arch_types.h"
#endif

#define ALIGN(x) [[gnu::aligned(x)]]

#ifdef __clang__
#define NONSTRING
#else
#define NONSTRING __attribute__((nonstring))
#endif

typedef volatile uint8_t IO8;
typedef volatile uint16_t IO16;
typedef volatile uint32_t IO32;

typedef ptrdiff_t ssize_t;
typedef ssize_t off_t;
