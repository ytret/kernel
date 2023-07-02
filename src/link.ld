ENTRY(entry)

SECTIONS
{
        . = 1M;
        ld_vmm_kernel_start = .;

        .text ALIGN(4K) :
        {
                *(.multiboot_header)
                *(.text)
        }

        .rodata ALIGN(4K) :
        {
                *(.rodata)
        }

        .data ALIGN(4K) :
        {
                *(.data)
        }

        .bss ALIGN(4K) :
        {
                *(COMMON)
                *(.bss)

                . = ALIGN(4K);
                ld_pmm_stack_bottom = .;
                . = 0x400000;   /* 4 MiB to cover 4 GiB */
                ld_pmm_stack_top = .;
        }

        ld_vmm_kernel_end = ALIGN(4K);
}
