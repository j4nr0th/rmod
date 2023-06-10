//
// Created by jan on 10.6.2023.
//

#include "internal_formatted.h"

char unsigned_get_lsd_dec_and_shift(uintmax_t* p_value)
{
    lldiv_t res = lldiv((long long)*p_value, 10);
    *p_value = res.quot;
    static_assert('0' + 1 == '1');
    assert(res.rem < 10);
    return (char)('0' + res.rem);
}

char unsigned_get_lsd_oct_and_shift(uintmax_t* p_value)
{
    uintmax_t v = *p_value;
    *p_value = v >> 3;
    static_assert('0' + 1 == '1');
    return (char)('0' + (v & 7));
}

char unsigned_get_lsd_hex_and_shift(uintmax_t* p_value)
{
    uintmax_t v = *p_value;
    *p_value = v >> 4;
    static_assert('0' + 1 == '1');
    v &= 15;
    return (char) (v > 9 ? 'a' + (v - 10) : '0' + v);
}

char unsigned_get_lsd_HEX_and_shift(uintmax_t* p_value)
{
    uintmax_t v = *p_value;
    *p_value = v >> 4;
    static_assert('0' + 1 == '1');
    v &= 15;
    return (char) (v > 9 ? 'A' + (v - 10) : '0' + v);
}

char double_get_dig_and_shift(double* p_value)
{
    double v = *p_value;
    assert(v >= 0.0 && v <= 10.0);
    const unsigned c = (unsigned)v;
    *p_value = (v - (double)c) * 10;
    return (char)(c + '0');
}

char double_get_hex_and_shift(double* p_value)
{
    double v = *p_value;
    assert(v >= 0.0 && v <= 16.0);
    const unsigned c = (unsigned)v;
    *p_value = ldexp(v - (double)c, -4);//(v - (double)c) * 16;
    return (char)(c > 9 ? (c - 10) + 'a' : c + '0');
}

char double_get_HEX_and_shift(double* p_value)
{
    double v = *p_value;
    assert(v >= 0.0 && v <= 16.0);
    const unsigned c = (unsigned)v;
    *p_value = ldexp(v - (double)c, -4);//(v - (double)c) * 16;
    return (char)(c > 9 ? (c - 10) + 'A' : c + '0');
}
