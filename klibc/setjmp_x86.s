                ## int setjmp(jmp_buf env)
                .global setjmp
                .type   setjmp, @function
setjmp:         mov     4(%esp), %eax   # eax = env

                mov     %ebx, 0(%eax)
                mov     %esi, 4(%eax)
                mov     %edi, 8(%eax)
                mov     %ebp, 12(%eax)

                lea     4(%esp), %edx   # future esp after return
                mov     %edx, 16(%eax)

                mov     0(%esp), %edx   # return address (eip)
                mov     %edx, 20(%eax)

                xor     %eax, %eax
                ret
                .size   setjmp, . - setjmp

                ## void longjmp(jmp_buf env, int val)
                .global longjmp
                .type   longjmp, @function
longjmp:        mov     4(%esp), %eax   # eax = env
                mov     8(%esp), %edx   # edx = val

                mov     0(%eax), %ebx
                mov     4(%eax), %esi
                mov     8(%eax), %edi
                mov     12(%eax), %ebp

                mov     16(%eax), %esp
                mov     20(%eax), %ecx  # ecx = target eip

                test    %edx, %edx
                jnz     .val_ok
                mov     $1, %edx        # replace val 0 with 1
.val_ok:        mov     %edx, %eax      # set return value for setjmp
                jmp     *%ecx
                .size   longjmp, . - longjmp
