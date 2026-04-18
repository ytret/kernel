#include "gdt.h"
#include "heap.h"
#include "memfun.h"

#define GDT_NUM_PRE_SMP_SEGS 3

static gdt_seg_desc_t g_gdt_pre_smp[GDT_NUM_PRE_SMP_SEGS];

static void prv_gdt_init_kernel_segs(gdt_seg_desc_t *gdt);
static void prv_gdt_set_base_limit(gdt_seg_desc_t *seg_desc, uint32_t base,
                                   uint32_t limit);

void gdt_init_pre_smp(gdtr_t *out_gdtr) {
    kmemset(g_gdt_pre_smp, 0, sizeof(g_gdt_pre_smp));
    prv_gdt_init_kernel_segs(g_gdt_pre_smp);

    out_gdtr->size = 3 * sizeof(g_gdt_pre_smp[0]) - 1;
    out_gdtr->addr = (uint32_t)&g_gdt_pre_smp[0];
}

void gdt_init_for_proc(gdt_seg_desc_t **out_gdt, tss_t **out_tss,
                       gdtr_t *out_gdtr) {
    /*
     * Per-processor GDTs have 6 segment descriptors:
     * - entry 0 is not used.
     * - entry 1 is kernel code.
     * - entry 2 is kernel data.
     * - entry 3 is user code.
     * - entry 4 is user data.
     * - entry 5 is task state segment.
     */

    static_assert(GDT_NUM_SMP_SEGS == 6);
    gdt_seg_desc_t *const gdt = heap_alloc(6 * sizeof(gdt_seg_desc_t));
    tss_t *const tss = heap_alloc(sizeof(tss_t));

    kmemset(gdt, 0, 6 * sizeof(gdt_seg_desc_t));
    kmemset(tss, 0, sizeof(tss_t));

    // Ring 0 (kernel) code and data.
    prv_gdt_init_kernel_segs(gdt);

    // Ring 3 (user) code.
    prv_gdt_set_base_limit(&gdt[3], 0, 0x00FFFFF);
    gdt[3].desc_type = GDT_DESC_TYPE_CODE_OR_DATA;
    gdt[3].seg_type = GDT_SEG_TYPE_CODE_RD;
    gdt[3].dpl = 3;
    gdt[3].present = 1;
    gdt[3].longm = 0;
    gdt[3].db = 1;
    gdt[3].gran = GDT_SEG_GRAN_4KB;

    // Ring 3 (user) data.
    prv_gdt_set_base_limit(&gdt[4], 0, 0x00FFFFF);
    gdt[4].desc_type = GDT_DESC_TYPE_CODE_OR_DATA;
    gdt[4].seg_type = GDT_SEG_TYPE_DATA_RW;
    gdt[4].dpl = 3;
    gdt[4].present = 1;
    gdt[4].longm = 0;
    gdt[4].db = 1;
    gdt[4].gran = GDT_SEG_GRAN_4KB;

    // Task state segment descriptor.
    static_assert(GDT_SMP_TSS_IDX == 5);
    prv_gdt_set_base_limit(&gdt[5], (uint32_t)tss, sizeof(tss_t));
    gdt[5].desc_type = GDT_DESC_TYPE_SYSTEM;
    gdt[5].seg_type = GDT_SEG_TYPE_TSS_32BIT;
    gdt[5].dpl = 0;
    gdt[5].present = 1;
    gdt[5].longm = 0;
    gdt[5].db = 1;
    gdt[5].gran = GDT_SEG_GRAN_BYTE;

    gdt_seg_sel_t tss_sel;
    tss_sel.index = 2;
    tss_sel.ti = 0;
    tss_sel.rpl = 0;
    kmemcpy(&tss->ss0, &tss_sel, sizeof(tss_sel));

    *out_gdt = gdt;
    *out_tss = tss;
    out_gdtr->size = 6 * sizeof(gdt_seg_desc_t) - 1;
    out_gdtr->addr = (uint32_t)gdt;
}

static void prv_gdt_init_kernel_segs(gdt_seg_desc_t *gdt) {
    // Ring 0 code.
    prv_gdt_set_base_limit(&gdt[1], 0, 0x00FFFFF);
    gdt[1].desc_type = GDT_DESC_TYPE_CODE_OR_DATA;
    gdt[1].seg_type = GDT_SEG_TYPE_CODE_RD;
    gdt[1].dpl = 0;
    gdt[1].present = 1;
    gdt[1].longm = 0;
    gdt[1].db = 1;
    gdt[1].gran = GDT_SEG_GRAN_4KB;

    // Ring 0 data.
    prv_gdt_set_base_limit(&gdt[2], 0, 0x00FFFFF);
    gdt[2].desc_type = GDT_DESC_TYPE_CODE_OR_DATA;
    gdt[2].seg_type = GDT_SEG_TYPE_DATA_RW;
    gdt[2].dpl = 0;
    gdt[2].present = 1;
    gdt[2].longm = 0;
    gdt[2].db = 1;
    gdt[2].gran = GDT_SEG_GRAN_4KB;
}

static void prv_gdt_set_base_limit(gdt_seg_desc_t *seg_desc, uint32_t base,
                                   uint32_t limit) {
    seg_desc->limit_15_0 = (uint16_t)limit;
    seg_desc->limit_19_16 = (limit >> 16) & 0x0F;

    seg_desc->base_15_0 = (uint16_t)base;
    seg_desc->base_23_16 = (uint8_t)(base >> 16);
    seg_desc->base_31_24 = (uint8_t)(base >> 24);
}
