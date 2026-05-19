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

    setjmp.s

)
target_compile_options(klibc PRIVATE
    -Wall -Wextra -Wmissing-prototypes
)
target_include_directories(klibc PUBLIC include)
target_include_directories(klibc PRIVATE ../include .)
