#pragma once

#include <stdint.h>

#define VADDR_MAX UINT32_MAX
#define PADDR_MAX UINT64_MAX

#define VIRT_TO_PHYS(x)                                                        \
    ((paddr_t)((uintptr_t)(x) - VMM_KERNEL_VMA + VMM_KERNEL_LMA))
#define PHYS_TO_VIRT(x) ((uint64_t)(x) - VMM_KERNEL_LMA + VMM_KERNEL_VMA)

typedef uint64_t paddr_t;
typedef uint32_t vaddr_t;

typedef uint32_t size_t;
