#pragma once

#include <stdbool.h>
#include <stdint.h>

bool elf_load(uint32_t * p_dir, void const * p_addr, uint32_t * p_entry);
void elf_dump(void const * p_addr);
