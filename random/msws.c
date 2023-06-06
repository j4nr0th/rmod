//
// Created by jan on 3.6.2023.
//

#include "msws.h"
#include <assert.h>

void rmod_msws_init(rmod_msws_state* rng, u64 x0, u64 x1, u64 w0, u64 w1)
{
    rng->x0 = x0;
    rng->x1 = x1;
    rng->w0 = w0;
    rng->w1 = x1;
    rng->s0 = 0xb5ad4eceda1ce2a9;
    rng->s1 = 0x278c5a4d8419fe6b;
    rng->has_remaining = false;
}

uint_fast64_t rmod_msws_rng(rmod_msws_state* rng)
{
    uint_fast64_t tmp;
    rng->x0 *= rng->x0;
    tmp = (rng->x0 += (rng->w0 += rng->s0));
    rng->x0 = ((rng->x0 >> 32)|(rng->x0 << 32));

    rng->x1 *= rng->x1;
    (rng->x1 += (rng->w1 += rng->s1));
    rng->x1 = ((rng->x1 >> 32)|(rng->x1 << 32));

    return tmp ^ rng->x1;
}

double rmod_msws_rngf(rmod_msws_state* rng)
{
    if (rng->has_remaining)
    {
        rng->has_remaining = false;
        return rng->remaining;
    }
    const union {
        uint_fast64_t v64;
        uint32_t v32[2];
    } v = {.v64 = rmod_msws_rng(rng)};
    static_assert(4294967296LLu == (1LLu << 32));
    static_assert((UINT32_MAX)/(4294967296.0) < 1.0);
    double f1 = ((double)(v.v32[1]) / (double)(4294967296));
    double f2 = ((double)(v.v32[0]) / (double)(4294967296));
//    float f1 = (float)(double)((double)(v.v64 & UINT32_MAX) / (double)(4294967296));
//    float f2 = (float)((double)(v.v64 >> 32) / (double)(4294967296));
    rng->has_remaining = true;
    rng->remaining = f1;
    return f2;
}

