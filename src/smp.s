                ## Application Processor (AP) trampoline.
                ##
                ## Initializes the running AP.
                ##
                ## This function has two assumptions:
                ##   1. It is run in real mode by an AP being initialized.
                ##   2. The page 0x9000..0x9FFF is used for stack.
                ##   3. No other AP is using the same stack.
                ##   4. Arguments (smp_ap_trampoline_args_t) are placed at the
                ##      physical address 0x8800.
                .code16
                .global smp_ap_trampoline
                .type   smp_ap_trampoline, @function
smp_ap_trampoline:
                cli
                cld

                ## Load the GDT descriptor at 0x8800, enable protected mode.
                mov     $0, %dx
                lgdt    0x8800
                mov     %cr0, %eax
                or      $1, %eax
                mov     %eax, %cr0
                ljmp    $0x08, $0x8020          # CS = 0x08

                ## NOTE: it is assumed that the alignment puts the code 64 bytes
                ## past the 'smp_ap_trampoline' label.
                .align  32
                .code32
_ap_trampoline_p2:
                ## Set the data segment registers to 0x10.
                mov     $0x10, %ax
                mov     %ax, %ds
                mov     %ax, %es
                mov     %ax, %fs
                mov     %ax, %gs
                mov     %ax, %ss

                ## Set up the initial stack.
                mov     $0x8800, %esi
                mov     6(%esi), %esp

                ## Call the C part. Do not use relative 'call' opcodes.
                call    $0x08, $smp_ap_trampoline_c
