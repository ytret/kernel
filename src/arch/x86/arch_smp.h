#include "acpi/acpi.h"
#include "arch/x86/gdt.h"

typedef struct {
    const acpi_proc_t *acpi;

    gdt_seg_desc_t *gdt;
    tss_t *tss;
    tss_t *df_tss;
    gdtr_t gdtr;

    void *df_stack_bottom;
    void *df_stack_top;
} arch_smp_proc_t;
