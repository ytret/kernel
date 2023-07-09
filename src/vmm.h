#pragma once

#define VMM_HEAP_START  0x400000
#define VMM_HEAP_SIZE   0x400000

void vmm_init(void);

// Defined in vmm.s.
//
void vmm_load_dir(void const * p_dir);
