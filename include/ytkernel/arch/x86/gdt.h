/**
 * @file gdt.h
 * Global Descriptor Table (GDT) management API.
 *
 * Refer to Intel(R) 64 and IA-32 Architectures Software Developer's Manual,
 * Volume 3 (3A, 3B, 3C & 3D): System Programming Guide, December 2021 (later
 * revisions have different chapter numbers).
 */

#pragma once

#include <stdint.h>

/// Number of segments used in per-processor GDTs.
#define GDT_NUM_SMP_SEGS 6

/// Index of the TSS descriptor in the GDT.
#define GDT_SMP_TSS_IDX 5

/**
 * Segment Selector.
 * Refer to section 3.4.2 "Segment Selectors".
 */
typedef struct [[gnu::packed]] {
    uint16_t rpl : 2; //!< Requested Privilege Level.
    uint16_t ti : 1;  //!< Table Indicator.
    uint16_t index : 13;
} gdt_seg_sel_t;

/**
 * Segment Selector, field Table Indicator.
 * See #gdt_seg_sel_t.ti.
 */
typedef enum {
    GDT_SEG_SEL_TI_GDT = 0,
    GDT_SEG_SEL_TI_LDT = 1,
} gdt_seg_sel_ti_t;

/**
 * Segment Descriptor.
 * Refer to section 3.4.5 "Segment Descriptors".
 */
typedef struct [[gnu::packed]] {
    uint32_t limit_15_0 : 16; //!< Segment Limit, bits 0..15.
    uint32_t base_15_0 : 16;  //!< Segment Base Address, bits 0..15.

    uint32_t base_23_16 : 8;  //!< Segment Base Address, bits 16..23.
    uint32_t seg_type : 4;    //!< Segment Type.
    uint32_t desc_type : 1;   //!< Descriptor Type.
    uint32_t dpl : 2;         //!< Descriptor Privilege Level.
    uint32_t present : 1;     //!< Segment Present.
    uint32_t limit_19_16 : 4; //!< Segment Limit, bits 16..19.
    uint32_t avl : 1;         //!< Available for use by system software.
    uint32_t longm : 1;       //!< 64-bit Code Segment (IA-32e mode only).
    uint32_t db : 1;          //!< Default Operation Size.
    uint32_t gran : 1;        //!< Granularity.
    uint32_t base_31_24 : 8;  //!< Segment Base Address, bits 24..31.
} gdt_seg_desc_t;

/**
 * Segment Descriptor, field Segment Type.
 *
 * If the descriptor is a system-type descriptor (bit #gdt_seg_desc_t.desc_type
 * is #GDT_DESC_TYPE_SYSTEM), then only the TSS value can be used.
 *
 * Otherwise, if the descriptor type is #GDT_DESC_TYPE_CODE_OR_DATA, any code or
 * data segment type can be used. In that case, bit 0 of code/data segment
 * descriptors is the Accessed flag, none of these enum values have it set.
 *
 * See #gdt_seg_desc_t.seg_type.
 */
typedef enum {
    GDT_SEG_TYPE_DATA_RO = 0b0000,         //!< Data, Read-Only.
    GDT_SEG_TYPE_DATA_RW = 0b0010,         //!< Data, Read/Write.
    GDT_SEG_TYPE_DATA_RO_EXPDOWN = 0b0100, //!< Data, Read-Only, expand-down.
    GDT_SEG_TYPE_DATA_RW_EXPDOWN = 0b0110, //!< Data, Read/Write, expand-down.
    GDT_SEG_TYPE_CODE_NRD = 0b1000,        //!< Code, Execute-Only.
    GDT_SEG_TYPE_CODE_RD = 0b1010,         //!< Code, Execute/Read.
    GDT_SEG_TYPE_CODE_NRD_CONF = 0b1100,   //!< Code, Execute-Only, conforming.
    GDT_SEG_TYPE_CODE_RD_CONF = 0b1110,    //!< Code, Execute/Read, conforming.

    GDT_SEG_TYPE_TSS_32BIT = 0b1001, //!< 32-bit TSS (Available).
} gdt_seg_type_t;

/**
 * Segment Descriptor, field Descriptor Type.
 * See #gdt_seg_desc_t.desc_type.
 */
typedef enum {
    GDT_DESC_TYPE_SYSTEM = 0,
    GDT_DESC_TYPE_CODE_OR_DATA = 1,
} gdt_desc_type_t;

/**
 * Segment Descriptor, field Default Operation Size.
 * See #gdt_seg_desc_t.db.
 */
typedef enum {
    GDT_SEG_DB_16BIT,
    GDT_SEG_DB_32BIT,
} gdt_seg_db_t;

/**
 * Segment Descriptor, filed Granularity.
 * See #gdt_seg_desc_t.gran.
 */
typedef enum {
    GDT_SEG_GRAN_BYTE,
    GDT_SEG_GRAN_4KB,
} gdt_seg_gran_t;

/**
 * Global Descriptor Table Register structure.
 * Refer to section 2.4.1 "Global Descriptor Table Register (GDTR)".
 */
typedef struct [[gnu::packed]] {
    uint16_t size;
    uint32_t addr;
} gdtr_t;

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

void gdt_init_pre_smp(gdtr_t *out_gdtr);
void gdt_init_for_proc(gdt_seg_desc_t **out_gdt, tss_t **out_tss,
                       gdtr_t *out_gdtr);

// Defined in gdt.s.
void gdt_load(const gdtr_t *gdtr);
