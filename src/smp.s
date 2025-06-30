                .code16

                .global smp_ap_trampoline
                .type   smp_ap_trampoline, @function
smp_ap_trampoline:
                cli
                cld
                ljmp    $0, $0x8040
                .size   smp_ap_trampoline, . - smp_ap_trampoline

                .align  0x40
prv_addr_0x8040:
                hlt
                jmp     prv_addr_0x8040
