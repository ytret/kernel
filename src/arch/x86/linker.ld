ENTRY(boot_entry)

PHDRS
{
        boot_text PT_LOAD FLAGS(5); /* R + E */
        boot_data PT_LOAD FLAGS(4); /* R only */
        text      PT_LOAD FLAGS(5); /* R + E */
        ro        PT_LOAD FLAGS(4); /* R only */
        data      PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
        ld_boot_vma = 1M;
        ld_kernel_vma = 0xC0000000;
        ld_kernel_lma = 4M; /* must be 4M-aligned, otherwise higher half mapping
                             * won't work */

        /**************/
        /* Boot stage */
        /**************/
        . = ld_boot_vma;

        .boot_text ALIGN(4K) : AT(ld_boot_vma)
        {
                KEEP(*(.multiboot_header))
                *(.boot_text)
        } :boot_text

        .boot_data ALIGN(4K) :
        {
                *(.boot_data)
        } :boot_data

        .boot_bss ALIGN(4K) :
        {
                ld_boot_bss_start = .;
                *(.boot_bss)
                ld_boot_bss_end = .;
        } :boot_data

        ld_boot_end = ALIGN(4K);

        ASSERT(ld_boot_end < ld_kernel_lma,
               "boot sections are too large, increase ld_kernel_lma")

        /**********************/
        /* Higher half kernel */
        /**********************/

        . = ld_kernel_vma;

        ld_vmm_kernel_start = .;

        .text ALIGN(4K) : AT(ld_kernel_lma)
        {
                *(.text)
        } :text

        .rodata ALIGN(4K) : AT(ld_kernel_lma + ADDR(.rodata) - ld_kernel_vma)
        {
                *(.rodata)
        } :ro

        .data ALIGN(4K) : AT(ld_kernel_lma + ADDR(.data) - ld_kernel_vma)
        {
                *(.data)
        } :data

        .bss ALIGN(4K) :
        {
                *(COMMON)
                *(.bss)
        } :data

        ld_vmm_kernel_end = ALIGN(4K);
}
