#pragma once

void vmm_init(void);

// Defined in vmm.s.
//
void vmm_load_dir(void const * p_dir);
