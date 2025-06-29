#pragma once

#include <stdint.h>

#define ALIGN(x) [[gnu::aligned(x)]]

typedef volatile uint8_t IO8;
typedef volatile uint16_t IO16;
typedef volatile uint32_t IO32;
