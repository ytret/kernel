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
        /**************/
        /* Boot stage */
        /**************/
        . = 1M;

        .boot_text ALIGN(4K) :
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

        /**********************/
        /* Higher half kernel */
        /**********************/

        . = 0xC0000000;

        ld_vmm_kernel_start = .;

        .text ALIGN(4K) : AT(LOADADDR(.boot_text) + SIZEOF(.boot_text))
        {
                *(.text)
        } :text

        .rodata ALIGN(4K) :
        {
                *(.rodata)
        } :ro

        .data ALIGN(4K) :
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
