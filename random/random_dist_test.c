//
// Created by jan on 2.6.2023.
//
#include "acorn.h"

#define ASSERT(x) if ((x) == false) {fprintf(stderr, "Failed assertion" #x "\n"); __builtin_trap(); exit(EXIT_FAILURE);}
#define N_BINS 1000
#define N_RUNS 1000000
int main()
{
    G_JALLOCATOR = jallocator_create((1 << 10), (1 << 9), 1);
    ASSERT(G_JALLOCATOR);
    rmod_acorn_state* acorn;
    rmod_result res = acorn_rng_create(60, 30, &acorn);
    ASSERT(res == RMOD_RESULT_SUCCESS);
    res = acorn_rng_seed(acorn, 130491230983509238LLU);
    ASSERT(res == RMOD_RESULT_SUCCESS);

    u32 bin_counts[N_BINS] = { 0 };
    for (u32 i = 0; i < N_RUNS; ++i)
    {
        const f64 v = acorn_rngf(acorn);
        const u32 idx = (u32) (v * (N_BINS));
        ASSERT(idx < N_BINS);
        bin_counts[idx] += 1;
    }

    for (u32 i = 0; i < N_BINS; ++i)
    {
        printf("Bin %u had %u\n", i, bin_counts[i]);
        ASSERT(bin_counts[i] > 800 && bin_counts[i] < 1200);
    }

    jallocator_destroy(G_JALLOCATOR);
    return 0;
}
