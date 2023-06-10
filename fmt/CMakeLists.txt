add_executable(formatted_string_test sformatted.c sformatted.h tests/fmt_test.c ../mem/lin_jalloc.c ../mem/lin_jalloc.h internal_formatted.c internal_formatted.h cformatted.c cformatted.h)
target_link_libraries(formatted_string_test PRIVATE m)
add_test(NAME formatted_test COMMAND formatted_string_test)