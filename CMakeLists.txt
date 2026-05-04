cmake_minimum_required(VERSION 3.19)
if(CMAKE_VERSION VERSION_GREATER 3.19)
    # CMP0115: Source file extensions must be explicit.
    # 3.20+
    cmake_policy(SET CMP0115 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER 3.23)
    # CMP0135: Do not extract timestamps from archives.
    # 3.24+
    cmake_policy(SET CMP0135 NEW)
endif()

project(kernel C ASM)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-O3 -g")


#############
## Options ##
#############
set(YTKERNEL_STACKTRACE_ON_PANIC ON CACHE BOOL
    "Print stacktrace on kernel panic.")
set(YTKERNEL_TERM_LOG_LEVEL 2 CACHE STRING
    "Maximum verbosity level of logs printed on the terminal.")
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/src/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
)


#################
## Kernel libc ##
#################
add_library(klibc STATIC

    klibc/ctype.c
    klibc/errno.c
    klibc/locale.c
    klibc/math.c
    klibc/stdio.c
    klibc/stdlib.c
    klibc/string.c
    klibc/time.c

    klibc/setjmp.s

)
target_compile_options(klibc PRIVATE
    -Wall -Wextra -Wmissing-prototypes
    ${OPTIMIZATION_FLAGS}
)
target_include_directories(klibc PUBLIC klibc/include)
target_include_directories(klibc PRIVATE include klibc)


##################
## Dependencies ##
##################
set(YTALLOC_BUILD_TESTS OFF CACHE BOOL "Disable building ytalloc tests.")
set(YTPRINTF_BUILD_TESTS OFF CACHE BOOL "Disable building ytprintf tests.")
include(FetchContent)
FetchContent_Declare(
    ytalloc
    GIT_REPOSITORY https://github.com/ytret/ytalloc.git
    GIT_TAG 95848d5a94dba199f1083fa3c272001ccbe8c2b8
    GIT_PROGRESS ON
)
FetchContent_Declare(
    ytprintf
    GIT_REPOSITORY https://github.com/ytret/ytprintf.git
    GIT_TAG 9ea3943d5dc345a2d41bc109d97b9dc4568e08f5
    GIT_PROGRESS ON
)
FetchContent_MakeAvailable(ytalloc)
FetchContent_MakeAvailable(ytprintf)
target_link_libraries(ytalloc klibc)
target_link_libraries(ytprintf klibc)

include(build_lua)
build_lua()


############
## Kernel ##
############
add_executable(kernel
    src/main.c

    src/acpi/acpi.c
    src/blkdev/ahci.c
    src/blkdev/blkdev.c
    src/blkdev/blkpart.c
    src/blkdev/gpt.c
    src/cpu.c
    src/devmgr.c
    src/elf.c
    src/framebuf.c
    src/fs/ramfs.c
    src/heap.c
    src/init.c
    src/kbd.c
    src/kprintf.c
    src/kstring.c
    src/ksyscall.c
    src/libshim.c
    src/log.c
    src/mbi.c
    src/memfun.c
    src/panic.c
    src/pci.c
    src/pic.c
    src/pit.c
    src/pmm.c
    src/psf.c
    src/serial.c
    src/slab.c
    src/smp.c
    src/stack.c
    src/taskmgr.c
    src/term.c
    src/vfs/file.c
    src/vfs/vfs_err.c
    src/vfs/vnode.c
    src/vfs/vpath.c
    src/vga.c

    src/arch/x86/apic/ioapic.c
    src/arch/x86/apic/lapic.c
    src/arch/x86/arch.c
    src/arch/x86/gdt.c
    src/arch/x86/idt.c
    src/arch/x86/vmm.c

    src/arch/x86/boot.s
    src/arch/x86/entry.s
    src/arch/x86/gdt.s
    src/arch/x86/idt.s
    src/arch/x86/isrs.s
    src/arch/x86/panic.s
    src/arch/x86/vmm.s

    src/taskmgr.s

    src/kshell/kbdlog.c
    src/kshell/ksharg.c
    src/kshell/kshcmd/ksh_clear.c
    src/kshell/kshcmd/kshcmd.c
    src/kshell/kshcmd/ksh_devmgr.c
    src/kshell/kshcmd/ksh_help.c
    src/kshell/kshcmd/ksh_lua.c
    src/kshell/kshcmd/ksh_mbi.c
    src/kshell/kshcmd/ksh_taskmgr.c
    src/kshell/kshcmd/ksh_vasview.c
    src/kshell/kshcmd/ksh_vfs.c
    src/kshell/kshell.c
    src/kshell/kshinput.c
    src/kshell/kshscan.c
    src/kshell/vasview.c

    src/kmutex.c
    src/ksemaphore.c
    src/kspinlock.c
    src/list.c
    src/queue.c

)
target_compile_options(kernel PRIVATE
    -Wall -Wextra -Wmissing-prototypes
    ${OPTIMIZATION_FLAGS}
)
target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    src
    include/ytkernel
)
target_link_options(kernel PRIVATE
    ${OPTIMIZATION_FLAGS}
    -T${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
)
target_link_libraries(kernel ytalloc ytprintf lua klibc gcc)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(kernel PRIVATE -DYTKERNEL_DEBUG)
endif()
add_dependencies(kernel lua54)
set_target_properties(kernel PROPERTIES
    C_EXTENSIONS ON
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
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


########################################
## Symlink tools into build directory ##
########################################
file(CREATE_LINK
    ${CMAKE_SOURCE_DIR}/tools/create_iso.sh
    ${CMAKE_BINARY_DIR}/create_iso.sh
    SYMBOLIC
)
file(CREATE_LINK
    ${CMAKE_SOURCE_DIR}/tools/run_qemu.sh
    ${CMAKE_BINARY_DIR}/run_qemu.sh
    SYMBOLIC
)
