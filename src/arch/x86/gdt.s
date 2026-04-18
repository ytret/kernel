                ## Global descriptor table functions.

                .global gdt_load
                .type   gdt_load, @function
gdt_load:       push    %ebp
                mov     %esp, %ebp
                mov     8(%ebp), %eax

                lgdt    (%eax)
                ljmp    $0x08, $1f
1:              mov     $0x10, %ax
                mov     %ax, %ds
                mov     %ax, %es
                mov     %ax, %fs
                mov     %ax, %gs
                mov     %ax, %ss

                pop     %ebp
                ret
                .size   gdt_load, . - gdt_load
