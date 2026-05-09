add_executable(user entry.s main.c)
target_compile_options(user PRIVATE
    -Wall -Wextra
)
target_link_options(user PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/link.ld
)
set_target_properties(user PROPERTIES
    C_EXTENSIONS OFF
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
)
