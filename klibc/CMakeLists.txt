#################
## Kernel libc ##
#################
add_library(klibc STATIC

    ctype.c
    errno.c
    locale.c
    math.c
    stdio.c
    stdlib.c
    string.c
    time.c

)
target_compile_options(klibc PRIVATE
    -Wall -Wextra -Wmissing-prototypes
)
target_include_directories(klibc PUBLIC include)
target_include_directories(klibc PRIVATE ../include .)

if(YTKERNEL_ARCH STREQUAL x86)
    target_sources(klibc PRIVATE
        setjmp_x86.s
    )
else()
    message(FATAL_ERROR
        "klibc does not support architecture '${YTKERNEL_ARCH}'")
endif()
