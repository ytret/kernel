                ## Example user program.

                .global userprog_entry
                .type   userprog_entry, @function
userprog_entry: jmp     userprog_entry
                .size   userprog_entry, . - userprog_entry
