/*
 * Declarations of interrupt service routines defined in isrs.s.
 */

#pragma once

#include <stdint.h>

typedef struct [[gnu::packed]] {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} isr_stack_frame_t;

typedef struct [[gnu::packed]] {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} isr_regs_t;

extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_9(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_15(void);
extern void isr_16(void);
extern void isr_17(void);
extern void isr_18(void);
extern void isr_19(void);
extern void isr_20(void);
extern void isr_21(void);
extern void isr_22(void);
extern void isr_23(void);
extern void isr_24(void);
extern void isr_25(void);
extern void isr_26(void);
extern void isr_27(void);
extern void isr_28(void);
extern void isr_29(void);
extern void isr_30(void);
extern void isr_31(void);

extern void isr_irq0(void);
extern void isr_irq1(void);
extern void isr_irq7(void);
extern void isr_irq15(void);

extern void isr_irq_ahci(void);

extern void isr_lapic_tim(void);
extern void isr_ipi_halt(void);
extern void isr_ipi_tlb_shootdown(void);

extern void isr_syscall(void);

extern void isr_dummy(void);
