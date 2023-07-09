#pragma once

#include <stdint.h>

#define VMM_HEAP_START  0x400000
#define VMM_HEAP_SIZE   0x400000

void vmm_init(void);

uint32_t * vmm_clone_kvas(void);

// Defined in vmm.s.
//
void vmm_load_dir(void const * p_dir);
