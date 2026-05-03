#pragma once

#include <stddef.h>
#include <stdint.h>

#define ALIGN(x) [[gnu::aligned(x)]]

#define VADDR_MAX UINT32_MAX
#define PADDR_MAX UINT64_MAX

#ifdef __clang__
#define NONSTRING
#else
#define NONSTRING __attribute__((nonstring))
#endif

typedef volatile uint8_t IO8;
typedef volatile uint16_t IO16;
typedef volatile uint32_t IO32;

typedef uint64_t paddr_t;
typedef uint32_t vaddr_t;

typedef uint32_t size_t;
typedef ptrdiff_t ssize_t;
typedef ssize_t off_t;
