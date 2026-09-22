cmake_minimum_required(VERSION 3.16)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

add_compile_options(-g -O0
        -std=gnu11
        -Wl,-Bsymbolic
        -rdynamic)

file(GLOB_RECURSE lists RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach (item ${lists})
    SET(code ${item} ${CMAKE_CURRENT_SOURCE_DIR}/../../source/crc16.c ${CMAKE_CURRENT_SOURCE_DIR}/../utils.c)
    STRING(REGEX REPLACE "\\.c" "" libName ${item})
    message(STATUS "plugin/compile/${item}")
    add_library(${libName} SHARED ${code})
    set_target_properties(${libName} PROPERTIES PREFIX "")
endforeach ()
