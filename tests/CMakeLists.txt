cmake_minimum_required(VERSION 3.19)
if(CMAKE_VERSION VERSION_GREATER 3.23)
    # CMP0135: Set timestamps of extracted downloaded archives to the extraction
    # time.
    cmake_policy(SET CMP0135 NEW) # 3.24+
endif()

project(kernel-tests C CXX)
enable_testing()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(SRCDIR ${CMAKE_CURRENT_LIST_DIR}/../src)
set(INCDIR ${CMAKE_CURRENT_LIST_DIR}/../include/ytkernel)

add_library(common STATIC
    ${SRCDIR}/list.c
)
target_compile_options(common PRIVATE
    -Wall -Wextra -fdiagnostics-color=always
)
target_include_directories(common PUBLIC ${SRCDIR} ${INCDIR})

add_library(test-fakes STATIC
    fakes/heap.c
    fakes/kprintf.c
    fakes/kstring.c
    fakes/memfun.c
)
target_compile_options(test-fakes PRIVATE
    -Wall -Wextra -fdiagnostics-color=always
)
target_include_directories(test-fakes PUBLIC fakes)

function(add_test)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES DEFINES)
    cmake_parse_arguments(PARSE_ARGV 0 arg
        "${options}" "${oneValueArgs}" "${multiValueArgs}"
    )

    set(name "${arg_NAME}")

    list(APPEND sources "${name}.cc")
    list(APPEND sources ${arg_SOURCES})

    list(APPEND compile_defs ${arg_DEFINES})

    add_executable(${name} ${sources})
    target_compile_options(${name} PRIVATE
        -Wall -Wextra -fdiagnostics-color=always
    )
    target_compile_definitions(${name} PRIVATE ${compile_defs})
    target_link_libraries(${name}
        test-fakes common GTest::gtest_main GTest::gmock_main
    )
    target_include_directories(${name} PRIVATE ${SRCDIR} ${INCDIR})

    gtest_discover_tests(${name})
endfunction()

add_test(NAME kshscan_test SOURCES ${SRCDIR}/kshell/kshscan.c)
add_test(
    NAME ksharg_test
    SOURCES ${SRCDIR}/kshell/ksharg.c ${SRCDIR}/kshell/kshscan.c
)
add_test(NAME vfspath_test SOURCES ${SRCDIR}/vfs/vpath.c)
add_test(NAME list_test SOURCES ${SRCDIR}/list.c)
