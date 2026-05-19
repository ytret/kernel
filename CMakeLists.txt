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
set(YTKERNEL_ARCH "x86" CACHE STRING "Target architecture (x86)")
set(YTKERNEL_STACKTRACE_ON_PANIC ON CACHE BOOL
    "Print stacktrace on kernel panic.")
set(YTKERNEL_ENABLE_TESTS ON CACHE BOOL "Enable the kernel test subsystem.")
set(YTKERNEL_TERM_LOG_LEVEL 2 CACHE STRING
    "Maximum verbosity level of logs printed on the terminal.")
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/src/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
)


#################
## Kernel libc ##
#################
add_subdirectory(klibc)


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


###################
## Kernel target ##
###################
add_executable(kernel)
# Look for target_sources(kernel ...) in subdirectories for kernel sources.
target_compile_options(kernel PRIVATE
    -Wall -Wextra -Wmissing-prototypes
)
target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    src
    include/ytkernel
)
target_link_libraries(kernel ytalloc ytprintf lua klibc gcc)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(kernel PRIVATE -DYTKERNEL_DEBUG)
endif()
add_dependencies(kernel lua54)
set_target_properties(kernel PROPERTIES
    C_EXTENSIONS ON
)


##########################
## Other subdirectories ##
##########################
# For klibc see above.
add_subdirectory(src)
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
file(CREATE_LINK
    ${CMAKE_SOURCE_DIR}/tools/ktest.sh
    ${CMAKE_BINARY_DIR}/ktest.sh
    SYMBOLIC
)
file(CREATE_LINK
    ${CMAKE_SOURCE_DIR}/tools/tapview.sh
    ${CMAKE_BINARY_DIR}/tapview.sh
    SYMBOLIC
)
