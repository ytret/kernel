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

                ## If p_from is not provided (NULL), then this is an entry into
                ## the very first task with the current stack being abandoned.
                ## Go straight to loading the next task's context, since there
                ## is no context to save.
                cmp     $0, 8(%ebp)
                jz      1f

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
                mov     4(%esi), %eax           # eax = p_from->p_kernel_stack
                mov     %esp, (%eax)            # first field is the stack top

                ## Load the parameters.
1:              mov     12(%ebp), %edi          # edi = p_to
                mov     16(%ebp), %eax          # eax = p_tss

                ## Load control block of the next task.
                mov     0(%edi), %ebx           # ebx = cr3
                mov     4(%edi), %esp
                mov     (%esp), %esp            # esp = kernel stack top

                ## Update TSS.ESP0 to point to the kernel stack top of the
                ## next task.
                mov     8(%ecx), %ecx
                mov     %ecx, 4(%eax)

                ## Change the virtual address space.
                mov     %cr3, %eax
                cmp     %ebx, %eax
                je      2f
                mov     %ebx, %cr3

                ## Pop kernel stack of the next task.
2:              pop     %edi
                pop     %esi
                pop     %ebx
                pop     %edx
                pop     %ecx
                pop     %eax
                pop     %ebp
                ret
                .size   taskmgr_switch_tasks, . - taskmgr_switch_tasks

                ##
                ## Does a far return with usermode segments to the specified
                ## function.
                ##
                ## Arguments:
                ##   1. uint32_t    user_code_seg
                ##   2. uint32_t    user_data_seg
                ##   3. uint32_t    tls_seg
                ##   4. uint32_t    entry_point
                ##   5. gp_regs_t * p_user_regs
                ##
                ## NOTE: does not return.
                ##
                .global taskmgr_go_usermode_impl
                .type   taskmgr_go_usermode_impl, @function
taskmgr_go_usermode_impl:
                push    %ebp
                mov     %esp, %ebp

                mov     8(%ebp), %eax           # eax = user_code_seg
                mov     12(%ebp), %ebx          # ebx = user_data_seg
                mov     16(%ebp), %ecx          # ecx = tls_seg
                mov     20(%ebp), %edx          # edx = entry_point
                mov     24(%ebp), %esi          # esi = p_user_regs

                ## Set RPL to 3, that is to usermode.
                or      $3, %eax
                or      $3, %ebx
                or      $3, %ecx

                ## Set the data segments (%ss is set by iret).
                mov     %bx, %ds
                mov     %bx, %es
                mov     %bx, %fs
                mov     %bx, %gs
                ## mov     %cx, %gs

                ## Set up the iret stack frame.
                push    %ebx                    # ss = user_data_seg
                push    3*4(%esi)               # esp
                pushf                           # eflags
                push    %eax                    # cs = user_code_seg
                push    %edx                    # eip = entry_point

                ## Load the general purpose registers.
                mov     0*4(%esi), %edi
                mov     2*4(%esi), %ebp
                mov     4*4(%esi), %ebx
                mov     5*4(%esi), %edx
                mov     6*4(%esi), %ecx
                mov     7*4(%esi), %eax
                mov     1*4(%esi), %esi

                iret
                .size   taskmgr_go_usermode_impl, . - taskmgr_go_usermode_impl
