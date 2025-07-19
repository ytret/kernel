cmake_minimum_required(VERSION 3.19)
if(CMAKE_VERSION VERSION_GREATER 3.23)
    # CMP0135: Set timestamps of extracted downloaded archives to the extraction
    # time.
    cmake_policy(SET CMP0135 NEW) # 3.24+
endif()

project(kernel-tests C CXX)

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

add_library(common STATIC
    ${SRCDIR}/list.c
)
target_compile_options(common PRIVATE
    -Wall -Wextra -fdiagnostics-color=always
)
target_include_directories(common PUBLIC ${SRCDIR})

add_library(fakes STATIC
    fakes/heap.c
    fakes/kstring.c
    fakes/memfun.c
)
target_compile_options(fakes PRIVATE
    -Wall -Wextra -fdiagnostics-color=always
)
target_include_directories(fakes PUBLIC fakes)

function(add_unit_test name mod extra_src)
    add_executable(${name}
        ${name}.cc
        ${mod}
        ${extra_src}
    )
    target_compile_options(${name} PRIVATE
        -Wall -Wextra -fdiagnostics-color=always
    )
    target_include_directories(${name} PRIVATE ${SRCDIR})
    target_link_libraries(${name}
        fakes common GTest::gtest_main GTest::gmock_main
    )
    gtest_discover_tests(${name})
endfunction()

add_unit_test(kshscan_test ${SRCDIR}/kshell/kshscan.c "")
add_unit_test(ksharg_test ${SRCDIR}/kshell/ksharg.c ${SRCDIR}/kshell/kshscan.c)
