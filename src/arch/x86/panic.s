                ## Panic stacktrace.

                ##
                ## int panic_walk_stack(uint32_t *arr_addr, uint32_t max_items)
                ##
                ## @param arr_addr  Array of addresses.
                ## @param max_items Maximum number of items in @a arr_addr.
                ##
                ## @returns Number of addresses placed in @a arr_addr.
                ##
                .global panic_walk_stack
                .type   panic_walk_stack, @function
panic_walk_stack:
                push    %ebp
                mov     %esp, %ebp

                ## Save edi and ebx.
                sub     $8, %esp
                mov     %edi, -4(%ebp)
                mov     %ebx, -8(%ebp)

                xor     %eax, %eax
                mov     8(%esp), %ebx   # ebx - old ebp
                mov     16(%esp), %edi  # edi - dest array pointer
                mov     20(%esp), %ecx  # ecx - max array size

.walk:          ## Walk backwards through ebp list.
                test    %ebx, %ebx
                jz      .done

                ## If paging is not enabled, do not bother with checking whether
                ## address in ebp is mapped.
                mov     %cr0, %edx
                and     $0x80000000, %edx
                jz      .save_eip

                ## Call vmm_is_addr_mapped(ebx) to determine whether it's safe
                ## to read (%ebp).
                push    %eax
                push    %ecx
                push    %edx
                push    %ebx            # arg - virt
                call    vmm_is_addr_mapped
                add     $4, %esp
                test    %eax, %eax
                jnz     .mapped
.not_mapped:    pop     %edx
                pop     %ecx
                pop     %eax
                jmp     .done
.mapped:
                pop     %edx
                pop     %ecx
                pop     %eax

.save_eip:      mov     4(%ebx), %edx   # edx - prev frame eip
                mov     0(%ebx), %ebx   # ebx - prev frame ebp
                mov     %edx, (%edi)    # save eip in arr_addr[]
                add     $4, %edi
                inc     %eax
                loop    .walk

.done:          ## Restore caller's edi and ebx.
                mov     -4(%ebp), %edi
                mov     -8(%ebp), %ebx
                add     $8, %esp

                pop     %ebp
                ret
