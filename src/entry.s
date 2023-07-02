                .set    FLAG_ALIGN,     1 << 0
                .set    FLAG_MEMINFO,   1 << 1

                .set    MAGIC,          0x1BADB002
                .set    FLAGS,          FLAG_ALIGN | FLAG_MEMINFO
                .set    CHECKSUM,       -(MAGIC + FLAGS)

                .section .multiboot_header
                .align  4
                .long   MAGIC
                .long   FLAGS
                .long   CHECKSUM

                .section .bss
                .align  16
stack_bottom:   .skip   16384
stack_top:

                .section .text
                .global entry
                .type   entry, @function
entry:          mov     $stack_top, %esp
                push    %ebx
                push    %eax
                call    main
                add     $8, %esp

                cli
1:              hlt
                jmp     1b
                .size   entry, . - entry
