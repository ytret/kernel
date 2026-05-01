                .set    FLAG_MODALIGN,  1 << 0
                .set    FLAG_MEMINFO,   1 << 1
                .set    FLAG_VIDEO,     1 << 2

                .set    MAGIC,    0x1BADB002
                .set    FLAGS,    FLAG_MODALIGN | FLAG_MEMINFO
                .set    CHECKSUM, -(MAGIC + FLAGS)

                // Preferred video mode.
                .set    VIDEO_MODE,     0
                .set    VIDEO_WIDTH,    0
                .set    VIDEO_HEIGHT,   0
                .set    VIDEO_DEPTH,    0

                .section .multiboot_header, "a"

                .align  4
                .long   MAGIC
                .long   FLAGS
                .long   CHECKSUM
                .skip   20
                .long   VIDEO_MODE
                .long   VIDEO_WIDTH
                .long   VIDEO_HEIGHT
                .long   VIDEO_DEPTH

                /**
                 * Numer of lower memory page tables to identity map.
                 *
                 * This number of page tables must be enough to map everything
                 * below the address of `ld_boot_end` (see the linker script).
                 */
                .set    NUM_LOWMEM_TABLES, 1
                /**
                 * Number of higher memory page tables.
                 *
                 * This number of page tables must be enough to map the whole
                 * kernel in the higher half. See symbols `ld_vmm_kernel_start`
                 * and `ld_vmm_kernel_end`.
                 */
                .set    NUM_HIMEM_TABLES, 4
                /**
                 * Base virtual address in higher memory for mapping.
                 *
                 * The higher memory region starting at `HIMEM_OFFSET` with the
                 * size `PGTBL_COVERAGE * NUM_HIMEM_TABLES` is mapped to the
                 * same physical region as the lower memory.
                 */
                .set    HIMEM_PGDIR_IDX, (0xC0000000 >> 22)
                .set    PAGE_SIZE, 4096
                .set    PGDIR_SIZE, 4096
                .set    PGTBL_SIZE, 4096
                .set    PGTBL_COVERAGE, (4096 * 1024)

                .section .boot_text, "ax"

                .global boot_entry
                .type   boot_entry, @function
boot_entry:
                // Save eax and ebx as they have special meaning.
                mov     %eax, (saved_eax)
                mov     %ebx, (saved_ebx)

                /*
                 * Check that NUM_LOWMEM_TABLES is enough to identity map the
                 * boot_text and boot_data segments.
                 * NOTE: it is assumed that ld_boot_end is aligned at 4K.
                 */
                mov     $ld_boot_end, %eax
                xor     %edx, %edx              // edx:eax = $ld_boot_end
                mov     $PGTBL_COVERAGE, %ebx
                div     %ebx                    // eax = edx:eax / ebx
                inc     %eax
                // eax = how many page tables are needed to identity map
                // 0x0000_0000 .. ld_boot_end.
                cmp     $NUM_LOWMEM_TABLES, %eax
                jg      halt                    // ld_boot_end is too high

                /*
                 * Clear everything in `.boot_bss`.
                 */
                mov     $0, %esi
                mov     $ld_boot_bss_start, %edi
                mov     $ld_boot_bss_end, %eax
1:
                mov     %esi, (%edi)
                add     $4, %edi
                cmp     %eax, %edi
                jl      1b

/******************************************************************************/
/************** Set up early stage paging *************************************/
/******************************************************************************/

                /*
                 * Identity map pages in the `boot_pgtbls` tables.
                 *
                 * Loop registers:
                 * - ecx - down counter of page table entries left to fill
                 * - edi - boot_pgtbls entry address
                 * - ebx - current physical page to map
                 * - edx - current page table entry
                 */
                mov     $NUM_LOWMEM_TABLES, %eax
                mov     $1024, %ebx
                mul     %ebx
                // eax = total number of entries in all page tables to set.
                mov     %eax, %ecx
                mov     $boot_pgtbls, %edi
                mov     $0, %ebx
1:
                mov     %ebx, %edx              // edx = physical address
                or      $3, %edx                // flags: present and writable
                mov     %edx, (%edi)
                add     $4, %edi
                add     $PAGE_SIZE, %ebx
                loop    1b

                /*
                 * Fill the page directory entries.
                 *
                 * Loop registers:
                 * - ecx - up counter of page dir entries left to fill
                 * - edi - boot_pgdir entry address
                 * - ebx - physical address of the current page table
                 * - edx - current page dir entry
                 */
                mov     $NUM_LOWMEM_TABLES, %eax
                mov     $0, %ecx
                mov     $boot_pgdir, %edi
                mov     $boot_pgtbls, %ebx
1:
                mov     %ebx, %edx
                or      $3, %edx                // flags: present and writable
                mov     %edx, (%edi)
                add     $4, %edi
                add     $PGTBL_SIZE, %ebx
                inc     %ecx
                cmp     %eax, %ecx
                jl      1b

/******************************************************************************/
/************** Map the higher half kernel ************************************/
/******************************************************************************/

                /*
                 * Map pages in the `himem_pgtbls` tables.
                 *
                 * Loop registers:
                 * - ecx - down counter of page table entries left to fill
                 * - edi - himem_pgtbls entry address
                 * - ebx - current physical page to map
                 * - edx - current page table entry
                 */
                mov     $NUM_HIMEM_TABLES, %eax
                mov     $1024, %ebx
                mul     %ebx
                // eax = total number of entries in all page tables to set.
                mov     %eax, %ecx
                mov     $himem_pgtbls, %edi
                mov     $ld_kernel_lma, %ebx
1:
                mov     %ebx, %edx              // edx = physical address
                or      $3, %edx                // flags: present and writable
                mov     %edx, (%edi)
                add     $4, %edi
                add     $PAGE_SIZE, %ebx
                loop    1b

                /*
                 * Fill the himem page directory entries.
                 *
                 * Loop registers:
                 * - ecx - up counter of page dir entries left to fill
                 * - edi - boot_pgdir entry address
                 * - ebx - physical address of the current page table
                 * - edx - current page dir entry
                 */
                mov     $NUM_HIMEM_TABLES, %eax
                mov     $0, %ecx
                mov     $boot_pgdir, %edi
                add     $(4 * HIMEM_PGDIR_IDX), %edi
                mov     $himem_pgtbls, %ebx
1:
                mov     %ebx, %edx
                or      $3, %edx                // flags: present and writable
                mov     %edx, (%edi)
                add     $4, %edi
                add     $PGTBL_SIZE, %ebx
                inc     %ecx
                cmp     %eax, %ecx
                jl      1b

                // Load the page directory and enable paging.
                mov     $boot_pgdir, %eax
                mov     %eax, %cr3
                mov     %cr0, %eax
                or      $0x80000001, %eax
                mov     %eax, %cr0

                // Jump to the higher half entry point.
                mov     (saved_eax), %eax
                mov     (saved_ebx), %ebx
                jmp     entry

halt:           hlt
                jmp     halt
                .size   boot_entry, . - boot_entry

                .section .boot_data, "aw"

                // We store these in .boot_data because .boot_bss gets cleared,
                // and we need to save these before it gets cleared.
saved_eax:      .skip 4
saved_ebx:      .skip 4

                .section .boot_bss, "aw", @nobits
                .align 4096

boot_pgdir:     .skip PGDIR_SIZE
boot_pgtbls:    .skip PGTBL_SIZE * NUM_LOWMEM_TABLES
boot_pgtbls_end:
himem_pgtbls:   .skip PGTBL_SIZE * NUM_HIMEM_TABLES

                .global boot_pgtbls
                .global boot_pgtbls_end
