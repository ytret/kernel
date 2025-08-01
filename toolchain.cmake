set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug|Release")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

execute_process(
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/tools/find-gcc-system-includes.sh"
    OUTPUT_VARIABLE SYSTEM_INCLUDE_DIRECTORY
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_C_FLAGS
    "-ffreestanding -nostdinc -isystem${SYSTEM_INCLUDE_DIRECTORY} -fdiagnostics-color=always"
)

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
