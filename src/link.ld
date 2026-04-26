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
        }

        ld_vmm_kernel_end = ALIGN(4K);
}
