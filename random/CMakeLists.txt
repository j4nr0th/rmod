add_executable(random_dist_test acorn.c acorn.h random_dist_test.c ../mem/jalloc.c ../mem/jalloc.h ../err/error_stack.c ../err/error_stack.h ../err/error_codes.c ../err/error_codes.h ../common.c msws.c msws.h)
target_link_libraries(random_dist_test PRIVATE m)
add_test(NAME random_distribution_test COMMAND random_dist_test)
