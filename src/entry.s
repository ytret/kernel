                .set    FLAG_MODALIGN,  1 << 0
                .set    FLAG_MEMINFO,   1 << 1
                .set    FLAG_VIDEO,     1 << 2

                .set    MAGIC,    0x1BADB002
                .set    FLAGS,    FLAG_MODALIGN | FLAG_MEMINFO
                .set    CHECKSUM, -(MAGIC + FLAGS)

                ## Preferred video mode.
                .set    VIDEO_MODE,     0
                .set    VIDEO_WIDTH,    0
                .set    VIDEO_HEIGHT,   0
                .set    VIDEO_DEPTH,    0

                .section .multiboot_header
                .align  4
                .long   MAGIC
                .long   FLAGS
                .long   CHECKSUM
                .skip   20
                .long   VIDEO_MODE
                .long   VIDEO_WIDTH
                .long   VIDEO_HEIGHT
                .long   VIDEO_DEPTH

                .section .bss

                ## Initial kernel stack.
                ##
                ## It is abandoned when the initial kernel task is entered,
                ## which has its own stack.
                .align  16
stack_bottom:   .skip   16384
stack_top:

                .section .text

                .global entry
                .type   entry, @function
entry:          mov     $stack_top, %esp
                push    %ebx
                push    %eax
                call    enable_sse
                call    main
                add     $8, %esp
                cli
1:              hlt
                jmp     1b
                .size   entry, . - entry

                .global enable_sse
                .type   enable_sse, @function
enable_sse:     push    %eax

                mov     %cr0, %eax
                ## Disable coprocessor emulation.
                and     $0xFFFFFFFB, %eax       # ~CR0.EM = ~(1 << 2)
                ## Enable coprocessor emulation.
                or      $0x2, %eax              # CR0.MP = 1 << 1
                mov     %eax, %cr0

                mov     %cr4, %eax
                ## Indicate kernel support for FXSAVE and FXRSTOR.
                or      $0x200, %eax            # CR4.OSFXSR = 1 << 9
                ## Indicate kernel support for unmasked SIMD FP exceptions.
                or      $0x400, %eax            # CR4.OSXMMEXCPT = 1 << 10
                mov     %eax, %cr4

                pop     %eax
                ret
                .size   enable_sse, . - enable_sse
