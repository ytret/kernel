                ## Virtual memory manager functions.

                .global vmm_load_dir
                .type   vmm_load_dir, @function
vmm_load_dir:   push    %ebp
                mov     %esp, %ebp

                ## Load the page directory.
                mov     8(%ebp), %eax
                mov     %eax, %cr3

                ## Enable paging.
                mov     %cr0, %eax
                or      $0x80000001, %eax
                mov     %eax, %cr0

                pop     %ebp
                ret
