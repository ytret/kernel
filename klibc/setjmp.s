                .global setjmp_impl
                .type   setjmp_impl, @function
setjmp_impl:    mov     4(%esp), %ecx
                mov     %ebp, 0(%ecx)
                mov     %esi, 4(%ecx)
                .size   setjmp_impl, . - setjmp_impl
