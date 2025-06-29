#pragma once

#include <stdint.h>

typedef struct [[gnu::packed]] {
    uint16_t prev;
    uint16_t reserved_prev;
    uint32_t esp0;
    uint16_t ss0;
    uint16_t reserved_ss0;
    uint32_t esp1;
    uint16_t ss1;
    uint16_t reserved_ss1;
    uint32_t esp2;
    uint16_t ss2;
    uint16_t reserved_ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es;
    uint16_t reserved_es;
    uint16_t cs;
    uint16_t reserved_cs;
    uint16_t ss;
    uint16_t reserved_ss;
    uint16_t ds;
    uint16_t reserved_ds;
    uint16_t fs;
    uint16_t reserved_fs;
    uint16_t gs;
    uint16_t reserved_gs;
    uint16_t ldtseg;
    uint16_t reserved_ldtseg;
    uint16_t reserved_iobp;
    uint16_t iobp;
    uint32_t ssp;
} tss_t;

void gdt_init(void);
tss_t *gdt_get_tss(void);

// Defined in gdt.s.
void gdt_load(uint8_t const *p_desc);
