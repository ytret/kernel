cmake_minimum_required(VERSION 3.19)
if(CMAKE_VERSION VERSION_GREATER 3.19)
    # CMP0115: Source file extensions must be explicit.
    # 3.20+
    cmake_policy(SET CMP0115 NEW)
endif()

project(kernel C ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-O3 -g")


############
## Kernel ##
############
add_executable(kernel
    src/main.c

    src/acpi/acpi.c
    src/acpi/ioapic.c
    src/acpi/lapic.c
    src/blkdev/ahci.c
    src/blkdev/blkdev.c
    src/blkdev/blkpart.c
    src/blkdev/gpt.c
    src/cpu.c
    src/devmgr.c
    src/elf.c
    src/framebuf.c
    src/gdt.c
    src/heap.c
    src/idt.c
    src/init.c
    src/kbd.c
    src/kprintf.c
    src/mbi.c
    src/memfun.c
    src/panic.c
    src/pci.c
    src/pic.c
    src/pit.c
    src/pmm.c
    src/psf.c
    src/smp.c
    src/stack.c
    src/string.c
    src/syscall.c
    src/taskmgr.c
    src/term.c
    src/vga.c
    src/vmm.c

    src/entry.s
    src/gdt.s
    src/idt.s
    src/isrs.s
    src/smp.s
    src/taskmgr.s
    src/vmm.s

    src/kshell/cmd.c
    src/kshell/kbdlog.c
    src/kshell/kshell.c
    src/kshell/vasview.c

    src/list.c
    src/mutex.c
    src/queue.c
    src/semaphore.c
    src/spinlock.c
)
target_compile_options(kernel PRIVATE
    -Wall -Wextra -Wmissing-prototypes
    ${OPTIMIZATION_FLAGS}
)
target_include_directories(kernel PRIVATE src)
target_link_options(kernel PRIVATE
    ${OPTIMIZATION_FLAGS}
    -T${CMAKE_SOURCE_DIR}/src/link.ld
)
set_target_properties(kernel PROPERTIES
    C_EXTENSIONS ON
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/link.ld
)


####################
## Subdirectories ##
####################
add_subdirectory(user)
add_subdirectory(docs)


###################################
## Copy files to build directory ##
###################################
configure_file(gdb/.gdbinit ${CMAKE_BINARY_DIR}/.gdbinit)
configure_file(gdb/kernel-gdb.scm ${CMAKE_BINARY_DIR}/kernel-gdb.scm)
configure_file(gdb/util.scm ${CMAKE_BINARY_DIR}/util.scm)
configure_file(gdb/linked-list.scm ${CMAKE_BINARY_DIR}/linked-list.scm)
configure_file(gdb/task.scm ${CMAKE_BINARY_DIR}/task.scm)
configure_file(gdb/heap.scm ${CMAKE_BINARY_DIR}/heap.scm)
