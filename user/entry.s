                .global entry
                .type   entry, @function
entry:          call    main
loop:           jmp     loop
                .size   entry, . - entry
