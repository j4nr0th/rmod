//
// Created by jan on 3.6.2023.
//

//  Based on:
//  Widynski, B., "Middle-Square Weyl Sequence RNG", 2022, https://doi.org/10.48550/arXiv.1704.00358

#ifndef RMOD_MSWS_H
#define RMOD_MSWS_H
#include <stdint.h>
#include <stdbool.h>

typedef struct rmod_msws_state_struct rmod_msws_state;
struct rmod_msws_state_struct
{
    uint_fast64_t x0, x1;
    uint_fast64_t w0, w1;
    uint_fast64_t s0, s1;
    bool has_remaining;
    double remaining;
};

void rmod_msws_init(rmod_msws_state* rng);

uint_fast64_t rmod_msws_rng(rmod_msws_state* rng);

double rmod_msws_rngf(rmod_msws_state* rng);


#endif //RMOD_MSWS_H
