//
// Created by jan on 10.6.2023.
//

#ifndef JSTR_INTERNAL_FORMATTED_H
#define JSTR_INTERNAL_FORMATTED_H
#include "../mem/lin_jalloc.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

char unsigned_get_lsd_dec_and_shift(uintmax_t* p_value);

char unsigned_get_lsd_oct_and_shift(uintmax_t* p_value);

char unsigned_get_lsd_hex_and_shift(uintmax_t* p_value);

char unsigned_get_lsd_HEX_and_shift(uintmax_t* p_value);

char double_get_dig_and_shift(double* p_value);

char double_get_hex_and_shift(double* p_value);

char double_get_HEX_and_shift(double* p_value);

#endif //JSTR_INTERNAL_FORMATTED_H
