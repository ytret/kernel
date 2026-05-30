#pragma once

#include <stddef.h>

#include "kerr.h"

kerr_t usermem_copy_to_k(void *kdst, const void *usrc, size_t size);
kerr_t usermem_copy_to_u(void *udst, const void *ksrc, size_t size);
