add_executable(random_dist_test acorn.c acorn.h random_dist_test.c ../mem/jalloc.c ../mem/jalloc.h ../err/error_stack.c ../err/error_stack.h ../err/error_codes.c ../err/error_codes.h)
add_test(NAME random_distribution_test COMMAND random_dist_test)
