//
// Created by jan on 2.6.2023.
//

#ifndef RMOD_ACORN_H
#define RMOD_ACORN_H
#include "../common.h"
#include "../rmod.h"
typedef struct rmod_acorn_state_struct rmod_acorn_state;
struct rmod_acorn_state_struct
{
    uint_fast64_t modulo;
    uint_fast64_t order;
    uint_fast64_t state[];
};

rmod_result acorn_rng_create(u64 modulo, u64 order, rmod_acorn_state** pp_out);

rmod_result acorn_rng_destroy(rmod_acorn_state* acorn);

rmod_result acorn_rng_seed(rmod_acorn_state* acorn, u64 seed);

uint_fast64_t acorn_rng(rmod_acorn_state* acorn);

f64 acorn_rngf(rmod_acorn_state* acorn);

#endif //RMOD_ACORN_H
