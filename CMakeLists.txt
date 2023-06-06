cmake_minimum_required(VERSION 3.25)
project(rmod C)

set(CMAKE_C_STANDARD 23)

list(APPEND JALLOC_SOURCE_FILES  mem/lin_jalloc.c mem/jalloc.c)
list(APPEND JALLOC_HEADER_FILES  mem/lin_jalloc.h mem/jalloc.h)
list(APPEND ERR_SOURCE_FILES     err/error_codes.c err/error_stack.c)
list(APPEND ERR_HEADER_FILES     err/error_codes.h err/error_stack.h)
list(APPEND STR_SOURCE_FILES     strings/formatted.c)
list(APPEND STR_HEADER_FILES     strings/formatted.h)

list(APPEND RMOD_SOURCE_FILES    platform.c compile.c program.c common.c simulation_run.c)
list(APPEND RMOD_HEADER_FILES    rmod.h     compile.h common.h program.h simulation_run.h)
list(APPEND RANDOM_SOURCE_FILES  random/acorn.c random/msws.c)
list(APPEND RANDOM_HEADER_FILES  random/acorn.h random/msws.h)
list(APPEND PARSING_SOURCE_FILES parsing_base.c graph_parsing.c config_parsing.c)
list(APPEND PARSING_HEADER_FILES parsing_base.h graph_parsing.h config_parsing.h)


if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options("-march=native")
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    add_compile_options("/arch:AVX2")
endif ()

if ((CMAKE_BUILD_TYPE STREQUAL Debug) OR (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo))
    add_compile_definitions(JALLOC_TRACKING)
endif ()

add_executable(rmod main.c
        ${JALLOC_SOURCE_FILES} ${JALLOC_HEADER_FILES}
        ${ERR_SOURCE_FILES} ${ERR_HEADER_FILES}
        ${RMOD_SOURCE_FILES} ${RMOD_HEADER_FILES}
        ${RANDOM_SOURCE_FILES} ${RANDOM_HEADER_FILES}
        #${STR_SOURCE_FILES} ${STR_HEADER_FILES}
        ${PARSING_SOURCE_FILES} ${PARSING_HEADER_FILES}
        )

target_link_libraries(rmod PRIVATE m pthread)
enable_testing()
add_subdirectory(random)
