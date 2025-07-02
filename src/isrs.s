                ## Interrupt service routines.

                .macro  ISR_EXCEPTION_NO_EC num c_fun
                .global isr_\num
                .type   isr_\num, @function
isr_\num:       cli
                push    %ebp
                mov     %esp, %ebp

                push    %eax
                push    %ecx
                push    %edx
                push    %ebx
                push    %edi
                push    %esi

                mov     %ebp, %edx
                add     $4, %edx
                push    %edx            # int stack frame     -> 3rd arg
                push    $0              # fake error code (0) -> 2nd arg
                push    $\num           # interrupt number    -> 1st arg
                cld
                call    \c_fun
                add     $12, %esp       # skip the arguments

                pop     %esi
                pop     %edi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax

                pop     %ebp
                iret
                .size   isr_\num, . - isr_\num
                .endm

                .macro  ISR_EXCEPTION_EC num c_fun
                .global isr_\num
                .type   isr_\num, @function
isr_\num:       cli
                push    %ebp
                mov     %esp, %ebp

                push    %eax
                push    %ecx
                push    %edx
                push    %ebx
                push    %edi
                push    %esi

                mov     %ebp, %edx
                add     $8, %edx
                push    %edx            # int stack frame  -> 3rd arg
                push    4(%ebp)         # error code       -> 2nd arg
                push    $\num           # interrupt number -> 1st arg
                cld
                call    \c_fun
                add     $12, %esp       # skip the arguments

                pop     %esi
                pop     %edi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax

                pop     %ebp
                add     $4, %esp        # skip the error code
                iret
                .size   isr_\num, . - isr_\num
                .endm

                .macro  ISR_EXCEPTION_DUMMY_NO_EC num
                ISR_EXCEPTION_NO_EC \num idt_dummy_exception_handler
                .endm

                .macro  ISR_EXCEPTION_DUMMY_EC num
                ISR_EXCEPTION_EC \num idt_dummy_exception_handler
                .endm

                ISR_EXCEPTION_DUMMY_NO_EC   0     # divide error
                ISR_EXCEPTION_DUMMY_NO_EC   1     # debug
                ISR_EXCEPTION_DUMMY_NO_EC   2     # non-maskable interrupt
                ISR_EXCEPTION_DUMMY_NO_EC   3     # breakpoint
                ISR_EXCEPTION_DUMMY_NO_EC   4     # overflow
                ISR_EXCEPTION_DUMMY_NO_EC   5     # bound range exceeded
                ISR_EXCEPTION_DUMMY_NO_EC   6     # invalid opcode
                ISR_EXCEPTION_DUMMY_NO_EC   7     # device not available
                ISR_EXCEPTION_DUMMY_EC      8     # double fault
                ISR_EXCEPTION_DUMMY_NO_EC   9     # coprocessor segment overrun
                ISR_EXCEPTION_DUMMY_EC      10    # invalid TSS
                ISR_EXCEPTION_DUMMY_EC      11    # segment not present
                ISR_EXCEPTION_DUMMY_EC      12    # stack fault
                ISR_EXCEPTION_DUMMY_EC      13    # general protection

                .global isr_14
                .type   isr_14, @function
isr_14:         cli
                push    %ebp
                mov     %esp, %ebp

                push    %eax
                push    %ecx
                push    %edx
                push    %ebx
                push    %edi
                push    %esi

                # 3rd arg - interrupt stack frame.
                mov     %ebp, %eax
                add     $8, %eax
                push    %eax

                # 2nd arg - error code.
                push    4(%ebp)

                # 1st arg - faulty address.
                mov     %cr2, %edx
                push    %edx

                cld
                call    idt_page_fault_handler
                add     $12, %esp       # skip the arguments

                pop     %esi
                pop     %edi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax

                pop     %ebp
                add     $4, %esp        # skip the error code
                iret
                .size   isr_14, . - isr_14


                ISR_EXCEPTION_DUMMY_NO_EC   15    # reserved
                ISR_EXCEPTION_DUMMY_NO_EC   16    # x87 FPU floating-point
                ISR_EXCEPTION_DUMMY_EC      17    # alignment check
                ISR_EXCEPTION_DUMMY_NO_EC   18    # machine check
                ISR_EXCEPTION_DUMMY_NO_EC   19    # SIMD floating-point
                ISR_EXCEPTION_DUMMY_NO_EC   20    # virtualization
                ISR_EXCEPTION_DUMMY_EC      21    # control protection
                ISR_EXCEPTION_DUMMY_NO_EC   22    # 22-31 reserved
                ISR_EXCEPTION_DUMMY_NO_EC   23
                ISR_EXCEPTION_DUMMY_NO_EC   24
                ISR_EXCEPTION_DUMMY_NO_EC   25
                ISR_EXCEPTION_DUMMY_NO_EC   26
                ISR_EXCEPTION_DUMMY_NO_EC   27
                ISR_EXCEPTION_DUMMY_NO_EC   28
                ISR_EXCEPTION_DUMMY_NO_EC   29
                ISR_EXCEPTION_DUMMY_NO_EC   30
                ISR_EXCEPTION_DUMMY_NO_EC   31

                ## ISR for IRQ0 (PIT).
                .global isr_irq0
                .type   isr_irq0, @function
isr_irq0:       cli
                push    %ebp
                mov     %esp, %ebp

                pusha
                cld
                ## NOTE: when changing the C handler name here, also update
                ## pic_spurious_irq_handler(), so that it calls the SAME handler
                ## as this ISR.
                call    pit_irq_handler
                popa

                pop     %ebp
                iret
                .size   isr_irq0, . - isr_irq0

                ## ISR for IRQ1 (keyboard).
                .global isr_irq1
                .type   isr_irq1, @function
isr_irq1:       cli
                push    %ebp
                mov     %esp, %ebp

                pusha
                cld
                ## NOTE: when changing the C handler name here, also update
                ## pic_spurious_irq_handler(), so that it calls the SAME handler
                ## as this ISR.
                call    kbd_irq_handler
                popa

                pop     %ebp
                iret
                .size   isr_irq1, . - isr_irq1

                ## ISR for IRQ7 (spurious IRQ).
                .global isr_irq7
                .type   isr_irq7, @function
isr_irq7:       cli
                push    %ebp
                mov     %esp, %ebp

                pusha
                push    $7
                cld
                call    pic_spurious_irq_handler
                add     $4, %esp
                popa

                pop     %ebp
                iret
                .size   isr_irq7, . - isr_irq7

                ## ISR for IRQ15 (spurious IRQ from slave PIC).
                .global isr_irq15
                .type   isr_irq15, @function
isr_irq15:      cli
                push    %ebp
                mov     %esp, %ebp

                pusha
                push    $15
                cld
                call    pic_spurious_irq_handler
                add     $4, %esp
                popa

                pop     %ebp
                iret
                .size   isr_irq15, . - isr_irq15

                ## Halt on panic ISR (inter-processor interrupt).
                .global isr_ipi_halt
                .type   isr_ipi_halt, @function
isr_ipi_halt:   cli
1:              hlt
                jmp     1b
                .size   isr_ipi_halt, . - isr_ipi_halt

                ## TLB Shootdown ISR (inter-processor interrupt).
                .global isr_ipi_tlb_shootdown
                .type   isr_ipi_tlb_shootdown, @function
isr_ipi_tlb_shootdown:
                cli
                push    %ebp
                mov     %esp, %ebp

                pusha
                cld
                call    smp_tlb_shootdown_handler
                popa

                pop     %ebp
                iret
                .size   isr_ipi_tlb_shootdown, . - isr_ipi_halt

                ## Syscall ISR.
                .global isr_syscall
                .type   isr_syscall, @function
isr_syscall:    cli
                push    %ebp
                mov     %esp, %ebp

                push    %eax
                push    %ecx
                push    %edx
                push    %ebx
                push    %esi
                push    %edi

                mov     %esp, %ebx
                push    %ebx            # registers -> 1st arg
                cld
                call    syscall_dispatch
                add     $4, %esp        # skip the argument

                pop     %edi
                pop     %esi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax

                pop     %ebp
                iret
                .size   isr_syscall, . - isr_syscall


                ## ISR for all the other interrupts.  Same as
                ## ISR_EXCEPTION_NO_EC, but does not push the interrupt number
                ## and error code.
                .global isr_dummy
                .type   isr_dummy, @function
isr_dummy:      cli
                push    %ebp
                mov     %esp, %ebp

                push    %eax
                push    %ecx
                push    %edx
                push    %edi
                push    %esi

                mov     %ebp, %edx
                add     $4, %edx
                push    %edx            # int stack frame -> 1st arg
                cld
                call    idt_dummy_handler
                add     $4, %esp        # skip the argument

                pop     %esi
                pop     %edi
                pop     %edx
                pop     %ecx
                pop     %eax

                pop     %ebp
                iret
                .size   isr_dummy, . - isr_dummy
