cmake_minimum_required(VERSION 3.25)
project(rmod C)

set(CMAKE_C_STANDARD 23)

list(APPEND JALLOC_SOURCE_FILES source/mem/lin_jalloc.c source/mem/jalloc.c)
list(APPEND JALLOC_HEADER_FILES source/mem/lin_jalloc.h source/mem/jalloc.h)
list(APPEND ERR_SOURCE_FILES source/err/error_codes.c source/err/error_stack.c)
list(APPEND ERR_HEADER_FILES source/err/error_codes.h source/err/error_stack.h)

list(APPEND RMOD_SOURCE_FILES source/common/platform.c source/simulation/compile.c source/simulation/program.c source/common/common.c source/simulation/simulation_run.c source/simulation/postprocessing.c)
list(APPEND RMOD_HEADER_FILES source/common/rmod.h source/simulation/compile.h source/common/common.h source/simulation/program.h source/simulation/simulation_run.h source/simulation/postprocessing.h)
list(APPEND RANDOM_SOURCE_FILES source/random/acorn.c source/random/msws.c)
list(APPEND RANDOM_HEADER_FILES source/random/acorn.h source/random/msws.h)
list(APPEND PARSING_SOURCE_FILES source/parsing/parsing_base.c source/parsing/graph_parsing.c source/parsing/config_parsing.c source/parsing/cli_parsing.c source/parsing/option_parsing.c)
list(APPEND PARSING_HEADER_FILES source/parsing/parsing_base.h source/parsing/graph_parsing.h source/parsing/config_parsing.h source/parsing/cli_parsing.h source/parsing/option_parsing.h)
list(APPEND FORMATTING_SOURCE_FILES source/fmt/sformatted.c source/fmt/cformatted.c source/fmt/sstream.c source/fmt/internal_formatted.c)
list(APPEND FORMATTING_HEADER_FILES source/fmt/sformatted.h source/fmt/cformatted.h source/fmt/sstream.h source/fmt/internal_formatted.h)


#   uncommenting these options makes the build specific to your CPU architecture, meaning on older processor might not
#   be able to run it, but will give you an observable speedup
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
#    add_compile_options("-march=native")
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
#    add_compile_options("/arch:AVX2")
endif ()

#if ((CMAKE_BUILD_TYPE STREQUAL Debug) OR (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo))
add_compile_definitions(JALLOC_TRACKING)
#endif ()

add_executable(rmod source/main.c
        ${JALLOC_SOURCE_FILES} ${JALLOC_HEADER_FILES}
        ${ERR_SOURCE_FILES} ${ERR_HEADER_FILES}
        ${RMOD_SOURCE_FILES} ${RMOD_HEADER_FILES}
        ${RANDOM_SOURCE_FILES} ${RANDOM_HEADER_FILES}
        ${PARSING_SOURCE_FILES} ${PARSING_HEADER_FILES}
        ${FORMATTING_SOURCE_FILES} ${FORMATTING_HEADER_FILES})


target_link_libraries(rmod PRIVATE m pthread)
enable_testing()
add_subdirectory(source/random)
add_subdirectory(source/fmt)
