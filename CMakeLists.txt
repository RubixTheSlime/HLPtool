cmake_minimum_required(VERSION 3.31)
project(HLPtool C)

# Set the minimum version of cmake required to build this project
cmake_minimum_required(VERSION 3.10)

# Use the package PkgConfig to detect GTK+ headers/library files
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtkmm-3.0)
pkg_check_modules(LUA REQUIRED lua5.4)


if (MSVC)
    add_compile_options(/arch:AVX2)
else ()
    add_compile_options(-mavx2 -mbmi -mbmi2 -mlzcnt)
endif ()

if ($ENV{RELEASE})
    add_compile_options(-O4)
endif ()

ADD_EXECUTABLE(hlpt
        src/aa_tree.c
        src/bitonic_sort.c
        src/command/dbin_command.c
        src/command/hex.c
        src/main.c
        src/redstone.c
        src/search/dbin_random.c
        src/search/hlp_random.c
        src/solver/dbin_solve.c
        src/solver/hlp_solve.c
        src/vector_tools.c
)


target_link_libraries(hlpt PRIVATE ${GTKMM_LIBRARIES})
target_link_libraries(hlpt PRIVATE ${GLIB_LIBRARIES})
target_link_libraries(hlpt PRIVATE ${LUA_LIBRARIES})

# Add other flags to the compiler
target_compile_definitions(hlpt PRIVATE ${GTKMM_CFLAGS_OTHER})
target_compile_definitions(hlpt PRIVATE ${GLIB_CFLAGS_OTHER})
target_compile_definitions(hlpt PRIVATE ${LUA_CFLAGS_OTHER})


# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
target_include_directories(hlpt PRIVATE ${GTKMM_INCLUDE_DIRS})
target_include_directories(hlpt PRIVATE ${GLIB_INCLUDE_DIRS})
target_include_directories(hlpt PRIVATE ${LUA_INCLUDE_DIRS})
target_link_directories(hlpt PRIVATE ${GTKMM_LIBRARY_DIRS})
target_link_directories(hlpt PRIVATE ${GLIB_LIBRARY_DIRS})
target_link_directories(hlpt PRIVATE ${LUA_LIBRARY_DIRS})
