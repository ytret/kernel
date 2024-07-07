#pragma once

#include <stdbool.h>
#include <stdint.h>

bool elf_load(uint32_t *p_dir, uint32_t addr, uint32_t *p_entry);
void elf_dump(uint32_t addr);
