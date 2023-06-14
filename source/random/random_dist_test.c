//
// Created by jan on 2.6.2023.
//
#include "msws.h"
#include "acorn.h"

#define ASSERT(x) if ((x) == false) {fprintf(stderr, "Failed assertion: \"" #x "\"\n"); __builtin_trap(); exit(EXIT_FAILURE);} (void)0
#define N_BINS 100
#define N_RUNS 100000
int main()
{
    G_JALLOCATOR = jallocator_create((1 << 10), (1 << 9), 1);
    ASSERT(G_JALLOCATOR);
    u32 bin_counts[N_BINS] = { 0 };

    u64 sum, sq_sum;


    //  Test for acorn rng
    rmod_acorn_state* acorn;
    rmod_result res = acorn_rng_create(60, 30, &acorn);
    ASSERT(res == RMOD_RESULT_SUCCESS);
    res = acorn_rng_seed(acorn, 130491230983509238LLU);
    ASSERT(res == RMOD_RESULT_SUCCESS);
#ifndef _WIN32
    struct timespec ts_begin;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_begin);
#else
#error NOT IMPLEMENTED
#endif
    for (u32 i = 0; i < N_RUNS; ++i)
    {
        const f64 v = acorn_rngf(acorn);
        const u32 idx = (u32) (v * (N_BINS));
        ASSERT(idx < N_BINS);
        bin_counts[idx] += 1;
    }
#ifndef _WIN32
    struct timespec ts_end;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_end);
    f64 duration = (f64)(ts_end.tv_sec - ts_begin.tv_sec) + ((f64)(ts_end.tv_nsec - ts_begin.tv_nsec) * 1e-9);
    printf("ACORN produced values with speed of %g values per ms\n", ((f64)N_RUNS) / (duration * 1e3));
#else
#error NOT IMPLEMENTED
#endif

    sum = 0, sq_sum = 0;
    for (u32 i = 0; i < N_BINS; ++i)
    {
//        printf("Bin %u had %u\n", i, bin_counts[i]);
        u64 bin_count = bin_counts[i];
        ASSERT(bin_count > ((f64)N_RUNS / (f64)N_BINS * 0.75) && bin_count < ((f64)N_RUNS / (f64)N_BINS * 1.25));
        sum += bin_count;
        sq_sum += bin_count * bin_count;
    }
    printf("ACORN measures of spread:\n\tBin count mean: %g\n\tBin count STD: %g\n", (f64)sum / N_BINS, sqrt((f64)(N_BINS * sq_sum - sum * sum) / (N_BINS * N_BINS)));
    acorn_rng_destroy(acorn);
    acorn = NULL;

    memset(bin_counts, 0, sizeof(bin_counts));
    rmod_msws_state msws;
    rmod_msws_init(&msws, 0, 0, 0, 0);
#ifndef _WIN32
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_begin);
#else
#error NOT IMPLEMENTED
#endif
    for (u32 i = 0; i < N_RUNS; ++i)
    {
        const f64 v = rmod_msws_rngf(&msws);
        u32 idx = (u32) (v * (N_BINS));
//        idx -= (idx == N_BINS);
        ASSERT(idx < N_BINS);
        bin_counts[idx] += 1;
    }
#ifndef _WIN32
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_end);
    duration = (f64)(ts_end.tv_sec - ts_begin.tv_sec) + ((f64)(ts_end.tv_nsec - ts_begin.tv_nsec) * 1e-9);
    printf("MSWS produced values with speed of %g values per ms\n", ((f64)N_RUNS) / (duration * 1e3));
#else
#error NOT IMPLEMENTED
#endif

    sum = 0, sq_sum = 0;
    for (u32 i = 0; i < N_BINS; ++i)
    {
        u64 bin_count = bin_counts[i];
//        printf("Bin %u had %u\n", i, bin_counts[i]);
        ASSERT(bin_counts[i] > ((f64)N_RUNS/(f64)N_BINS * 0.8) && bin_counts[i] < ((f64)N_RUNS/(f64)N_BINS * 1.2));
        sum += bin_count;
        sq_sum += bin_count * bin_count;
    }
    printf("MSWS measures of spread:\n\tBin count mean: %g\n\tBin count STD: %g\n", (f64)sum / N_BINS, sqrt((f64)(N_BINS * sq_sum - sum * sum) / (N_BINS * N_BINS)));


    jallocator_destroy(G_JALLOCATOR);
    return 0;
}
