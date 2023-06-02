//
// Created by jan on 2.6.2023.
//

//  ACORN PRNG was devised by Roy S. Wikramaratna in the 1980s.
//  The theory behind it is detailed on https://acorn.wikramaratna.org/ accessed on 2.6.2023
//

#include "acorn.h"


rmod_result acorn_rng_create(u64 modulo, u64 order, rmod_acorn_state** pp_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;

    rmod_acorn_state* this = jalloc(sizeof*this + sizeof(*this->state) * order);
    if (!this)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof*this + sizeof(*this->state) * order);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    if (order < 10)
    {
        RMOD_WARN("ACORN's order is %llu, but it is recommended have it be above 10", (unsigned long long)order);
    }

    if (modulo < (60))
    {
        RMOD_WARN("ACORN's modulo is equal to %llu, but it is recommended have it be as 2^60", 1LLu << ((unsigned long long)modulo));
    }

    memset(this, 0, sizeof*this + sizeof(*this->state) * order);
    this->order = order;
    this->modulo = (1LLu << modulo) - 1LLu;
    *pp_out = this;
end:
    RMOD_LEAVE_FUNCTION;
    return res;
}

rmod_result acorn_rng_destroy(rmod_acorn_state* acorn)
{
    RMOD_ENTER_FUNCTION;
    jfree(acorn);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

rmod_result acorn_rng_seed(rmod_acorn_state* acorn, u64 seed)
{
    RMOD_ENTER_FUNCTION;

    if (seed > acorn->modulo)
    {
        RMOD_WARN("Seed passed to %s was %llu, which is more than it's modulo of %llu", __func__, (unsigned long long) seed, (unsigned long long)acorn->modulo + 1);
    }
    if ((seed & 1) == 0)
    {
        RMOD_WARN("Seed passed to %s was even, which makes it not very prime relative to its seed", __func__);
    }
    acorn->state[0] = seed & acorn->modulo;

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

uint_fast64_t acorn_rng(rmod_acorn_state* acorn)
{
    RMOD_ENTER_FUNCTION;

    for (u32 i = 1; i < acorn->order; ++i)
    {
        acorn->state[i] = (acorn->state[i] + acorn->state[i - 1]) & acorn->modulo;
    }

    RMOD_LEAVE_FUNCTION;
    return acorn->state[acorn->order - 1];
}

f64 acorn_rngf(rmod_acorn_state* acorn)
{
    RMOD_ENTER_FUNCTION;
    const uint_fast64_t v = acorn_rng(acorn);
    RMOD_LEAVE_FUNCTION;
    return (f64)v / (f64)(acorn->modulo);
}
