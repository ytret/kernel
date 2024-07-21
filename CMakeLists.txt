cmake_minimum_required(VERSION 3.18)

project(kernel C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-O3")


############
## Kernel ##
############
add_executable(kernel
    src/main.c
    src/ahci.c
    src/elf.c
    src/framebuf.c
    src/gdt.c
    src/gpt.c
    src/heap.c
    src/idt.c
    src/kbd.c
    src/kprintf.c
    src/mbi.c
    src/memcpy.c
    src/memmove.c
    src/memset.c
    src/panic.c
    src/pci.c
    src/pic.c
    src/pit.c
    src/pmm.c
    src/psf.c
    src/stack.c
    src/string.c
    src/taskmgr.c
    src/term.c
    src/vga.c
    src/vmm.c
    src/entry.s
    src/gdt.s
    src/idt.s
    src/isrs.s
    src/taskmgr.s
    src/vmm.s
    src/kshell/cmd.c
    src/kshell/kshell.c
    src/kshell/vasview.c
)
target_compile_options(kernel PRIVATE
    -Wall -Wextra
    ${OPTIMIZATION_FLAGS}
)
target_include_directories(kernel PRIVATE src)
target_link_options(kernel PRIVATE
    ${OPTIMIZATION_FLAGS}
    -T${CMAKE_SOURCE_DIR}/src/link.ld
)
set_target_properties(kernel PROPERTIES
    C_EXTENSIONS OFF
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/link.ld
)
