//
// Created by jan on 13.5.2023.
//
#include <stdio.h>
#include <assert.h>
#include <float.h>
#include "../../mem/lin_jalloc.h"
#include "../sformatted.h"
#include "../cformatted.h"
#include <stdlib.h>
#include <time.h>

//  Note: specifier %p with snprintf does not match with lin_sprintf, because snprintf does not print leading zeros and
//        uses only lower case hex and 'x', but lin_sprintf uses upper case hex and 'x' with leading zeros
#ifdef NDEBUG
#undef assert
#define assert(statement) if (!(statement)) {fprintf(stderr, "Failed runtime assertion \"" #statement "\"\n"); exit(EXIT_FAILURE);} (void)0
#endif

int main()
{
    linear_jallocator* allocator = lin_jallocator_create(1 << 16);
    const char* const fmt_string = "C%#-6.8llX - %10G %12.30G %+025.015d %n si se%cora %p";
    char* sample_buffer = lin_jalloc(allocator, 1024);
    int so_far_1 = 0, so_far_2 = 0, so_far_3 = 0;

    size_t l_compare;

    char* comparison = lin_sprintf(allocator, &l_compare, fmt_string, 13ll, 130.0, DBL_EPSILON, 0xDEADBEEF, &so_far_2, u'\u00F1', sample_buffer);

    printf("Converted value by lin_sprintf was \"%s\" (%zu bytes long)\n", comparison, l_compare);
    lin_jfree(allocator, comparison);


    size_t l_base = snprintf(sample_buffer, 1024, fmt_string, 13ll, 130.0, DBL_EPSILON, 0xDEADBEEF, &so_far_1, u'\u00F1', sample_buffer);

    printf("Converted value by snprintf was \"%s\" (%zu bytes long)\n", sample_buffer, l_base);

    comparison = lin_sprintf(allocator, &l_compare, fmt_string, 13ll, 130.0, DBL_EPSILON, 0xDEADBEEF, &so_far_2, u'\u00F1', sample_buffer);

    printf("Converted value by lin_sprintf was \"%s\" (%zu bytes long)\n", comparison, l_compare);
    lin_eprintf(allocator, "Converted value by lin_sprintf was \"%s\" (%zu bytes long)\n", comparison, l_compare);
    assert(so_far_2 == so_far_1);
    assert(l_compare == l_base);
    assert(strcmp(sample_buffer, comparison) == 0);
    lin_jfree(allocator, comparison);

    size_t l_count = cprintf(allocator, fmt_string, 13ll, 130.0, DBL_EPSILON, 0xDEADBEEF, &so_far_3, u'\u00F1', sample_buffer);
    assert(l_count == l_base);
    assert(so_far_2 == so_far_3);

    lin_jallocator_destroy(allocator);
    return 0;
}
