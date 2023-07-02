                ## Interrupt descriptor table functions.

                .global idt_load
                .type   idt_load, @function
idt_load:       push    %ebp
                mov     %esp, %ebp
                mov     8(%ebp), %eax

                lidt    (%eax)

                pop     %ebp
                ret
                .size   idt_load, . - idt_load
