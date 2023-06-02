cmake_minimum_required(VERSION 3.25)
project(rmod C)

set(CMAKE_C_STANDARD 23)

list(APPEND JALLOC_SOURCE_FILES mem/lin_jalloc.c mem/jalloc.c)
list(APPEND JALLOC_HEADER_FILES mem/lin_jalloc.h mem/jalloc.h)
list(APPEND ERR_SOURCE_FILES err/error_codes.c err/error_stack.c)
list(APPEND ERR_HEADER_FILES err/error_codes.h err/error_stack.h)

list(APPEND RMOD_SOURCE_FILES graph_parsing.c compile.c platform.c program.c random/acorn.c)
list(APPEND RMOD_HEADER_FILES rmod.h  graph_parsing.h  compile.h common.h program.h random/acorn.h)

add_executable(rmod main.c ${JALLOC_SOURCE_FILES} ${JALLOC_HEADER_FILES} ${ERR_SOURCE_FILES} ${ERR_HEADER_FILES} ${RMOD_SOURCE_FILES} ${RMOD_HEADER_FILES})

enable_testing()
add_subdirectory(random)
