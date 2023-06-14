add_executable(formatted_string_test ../fmt/sformatted.c ../fmt/sformatted.h ../fmt/tests/fmt_test.c ../mem/lin_jalloc.c ../mem/lin_jalloc.h ../fmt/internal_formatted.c ../fmt/internal_formatted.h ../fmt/cformatted.c ../fmt/cformatted.h)
target_link_libraries(formatted_string_test PRIVATE m)
add_test(NAME formatted_test COMMAND formatted_string_test)