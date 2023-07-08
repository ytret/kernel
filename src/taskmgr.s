                ## Task manager functions.

                ##
                ## Passes execution from this task to the specified one.
                ## Updates the ESP0 field in the provided TSS.
                ##
                ## Arguments:
                ##   1. tcb_t       * p_from
                ##   2. tcb_t const * p_to
                ##   3. tss_t       * p_tss
                ##
                ## NOTE: the caller must disable interrupts before and enable
                ## them after calling this function.  Before - because the stack
                ## would corrupt, and after - for the task switch to happen
                ## again.
                ##
                .global taskmgr_switch_tasks
                .type   taskmgr_switch_tasks, @function
taskmgr_switch_tasks:
                push    %ebp
                mov     %esp, %ebp

                ## EBP already saved.  Push all the other registers onto the
                ## kernel stack of this task.
                push    %eax
                push    %ecx
                push    %edx
                push    %ebx
                push    %esi
                push    %edi

                ## Save ESP of this task in its control block.
                mov     8(%ebp), %esi           # esi = p_from
                mov     4(%esi), %eax           # eax = p_from_>p_kernel_stack
                mov     %esp, (%eax)            # first field is the stack top

                ## Load the parameters.
                mov     12(%ebp), %edi          # edi = p_to
                mov     16(%ebp), %eax          # eax = p_tss

                ## Load control block of the next task.
                mov     0(%edi), %ebx           # ebx = cr3
                mov     4(%edi), %esp
                mov     (%esp), %esp            # esp = kernel stack top

                ## Update TSS.ESP0.
                ## mov     %esp, 4(%eax)

                ## Change the virtual address space.
                ## TODO

                ## Pop kernel stack of the next task.
                pop     %edi
                pop     %esi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax
                pop     %ebp
                ret
                .size   taskmgr_switch_tasks, . - taskmgr_switch_tasks
