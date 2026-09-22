cmake_minimum_required(VERSION 3.25)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_compile_options(-g -O0
        -std=gnu11
        -Wl,-Bsymbolic
        -rdynamic)

file(GLOB_RECURSE lists RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach (item ${lists})
    STRING(REGEX REPLACE "\\.c" "" exeName ${item})
    message(STATUS "sub/${item}")
    add_executable(${exeName} ${item})
endforeach ()
