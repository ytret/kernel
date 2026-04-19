include(ExternalProject)

set(LUA_MYCFLAGS
    "${CMAKE_C_FLAGS} -O0 -g -I${CMAKE_CURRENT_LIST_DIR}/../klibc/include")

function(build_lua)
    ExternalProject_Add(lua54
        URL "https://www.lua.org/ftp/lua-5.4.7.tar.gz"
        URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make CC="${CMAKE_C_COMPILER}" MYCFLAGS=${LUA_MYCFLAGS} -C src a
        BUILD_IN_SOURCE TRUE
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS lua54-prefix/src/lua54/src/liblua.a
        DEPENDS klibc
    )
    ExternalProject_Get_Property(lua54 SOURCE_DIR)

    add_library(lua STATIC IMPORTED)
    set_target_properties(lua PROPERTIES
        IMPORTED_LOCATION ${SOURCE_DIR}/src/liblua.a
    )

    # HACK: create ${SOURCE_DIR}/src/ beforehand, so that CMake sees it during
    # the generation phase. (The Lua sources are downloaded later, during the
    # build phase.)
    #file(MAKE_DIRECTORY ${SOURCE_DIR}/src)
    target_include_directories(lua INTERFACE
        ${SOURCE_DIR}/src
    )
endfunction()
