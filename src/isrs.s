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
                push    %edi
                push    %esi

                mov     %ebp, %edx
                add     $4, %edx
                push    %edx            # int stack frame  -> 3rd arg
                push    4(%ebp)         # error code       -> 2nd arg
                push    $\num           # interrupt number -> 1st arg
                cld
                call    \c_fun
                add     $12, %esp       # skip the arguments

                pop     %esi
                pop     %edi
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
                ISR_EXCEPTION_DUMMY_EC      14    # page fault
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
                push    %edx            # int stack frame     -> 1st arg
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
