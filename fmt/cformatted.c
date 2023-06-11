//
// Created by jan on 10.6.2023.
//
#include "cformatted.h"
#include "internal_formatted.h"
#include <wchar.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

size_t cprintf(linear_jallocator* support_allocator, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const size_t ret_v = vacprintf(support_allocator, fmt, args);
    va_end(args);
    return ret_v;
}

size_t vacprintf(linear_jallocator* support_allocator, const char* fmt, va_list args)
{
    size_t used = 0;
    for (const char* ptr = fmt; *ptr; ++ptr)
    {
        if (*ptr == '%' && *(ptr += 1) != '%')
        {
            //  Time to format the string
            //  Parse the format specifier
            int flag_left_justify = 0;
            int flag_sign_pre_appended = 0;
            int flag_space_pre_appended = 0;
            int flag_alternative_conversion = 0;
            int flag_pad_leading_zeros = 0;
            int min_width = 0;
            int precision = 0;
            int precision_set = 0;

            //  Possible length modifiers
            //  hh
            //  h
            //  (none)
            //  l
            //  ll
            //  j
            //  z
            //  t
            //  L
            enum length_specifier_enum
            {
                LENGTH_MOD_NONE = 0,
                LENGTH_MOD_h = 1 << 0,
                LENGTH_MOD_hh = 1 << 1,
                LENGTH_MOD_l = 1 << 2,
                LENGTH_MOD_ll = 1 << 3,
                LENGTH_MOD_j = 1 << 4,
                LENGTH_MOD_z = LENGTH_MOD_j + 1,
                LENGTH_MOD_t = LENGTH_MOD_j + 2,
                LENGTH_MOD_L = LENGTH_MOD_j + 3,
            } length = LENGTH_MOD_NONE;


            //  Check for any flags
            flag_check:
            switch (*ptr)
            {
            case '-':
                flag_left_justify = 1;
                flag_pad_leading_zeros = 2;
                ptr += 1;
                goto flag_check;
            case '+':
                flag_sign_pre_appended = 1;
                ptr += 1;
                goto flag_check;
            case ' ':
                flag_space_pre_appended = 1;
                ptr += 1;
                goto flag_check;
            case '#':
                flag_alternative_conversion = 1;
                ptr += 1;
                goto flag_check;
            case '0':
                flag_pad_leading_zeros = flag_pad_leading_zeros?: 1;
                ptr += 1;
                goto flag_check;
            default:break;
            }
            if (flag_pad_leading_zeros == 2)
            {
                flag_pad_leading_zeros = 0;
            }
            if (!*ptr) break;

            //  Check minimum field width
            if (*ptr == '*')
            {
                //  The minimum width is given as a separate argument
                min_width = va_arg(args, int);
                if (min_width < 0)
                {
                    flag_left_justify = 1;
                    min_width = -min_width;
                }
                ptr += 1;
            }
            else if (*ptr >= '1' && *ptr <= '9')
            {
                //  Minimum width is given as actual value in the string
                do
                {
                    min_width = ((min_width << 3) + (min_width << 1));  //  Multiply by 10
                    min_width += *ptr - '1' + 1;    //  Add numeric value
                    ptr += 1;
                }
                while (*ptr >= '0' && *ptr <= '9');
            }
            if (!*ptr) break;

            //  Check for precision specifier
            if (*ptr == '.')
            {
                precision_set = 1;
                ptr += 1;
                if (*ptr == '*')
                {
                    //  Precision is given as separate argument
                    precision = va_arg(args, int);
                    if (precision < 0) precision = 0;
                    ptr += 1;
                }
                else if (*ptr >= '0' && *ptr <= '9')
                {
                    //  Minimum precision is given as actual value in the string
                    do
                    {
                        precision = ((precision << 3) + (precision << 1));  //  Multiply by 10
                        precision += *ptr - '1' + 1;    //  Add numeric value
                        ptr += 1;
                    }
                    while (*ptr >= '0' && *ptr <= '9');
                }
            }
            if (precision_set == 0)
            {
                precision = 1;
            }
            if (!*ptr) break;


            //  Check for length specifier
            switch (*ptr)
            {
            case 'h':
                //  If true, ptr needs to advance one forward
                length = *(ptr += 1) == 'h' ? ++ptr, LENGTH_MOD_hh : LENGTH_MOD_h;
                break;
            case 'l':
                //  If true, ptr needs to advance one forward
                length = *(ptr += 1) == 'l' ? ++ptr, LENGTH_MOD_ll : LENGTH_MOD_l;
                break;
            case 'j':
                length = LENGTH_MOD_j;
                ptr += 1;
                break;
            case 'z':
                length = LENGTH_MOD_z;
                ptr += 1;
                break;
            case 't':
                length = LENGTH_MOD_t;
                ptr += 1;
                break;
            case 'L':
                length = LENGTH_MOD_L;
                ptr += 1;
                break;
            default:break;
            }
            if (!*ptr) break;
            //  Now only thing left is the conversion specifier



            int clear_trailing_zeros = 0;
            double d_abnorm;
            switch (*ptr)
            {
            case 'c':
                //  Single character
                if (length == LENGTH_MOD_l)
                {
                    wchar_t v = va_arg(args, wint_t);
                    char buffer[8];
                    int len = wctomb(buffer, v);
                    used += len;
                }
                else
                {
                    int v = (unsigned char)va_arg(args, int);
                    used += 1;
                }
                break;

            case 's':
                if (length == LENGTH_MOD_l)
                {
                    //  Wide string
                    const wchar_t* wstr = va_arg(args, const wchar_t*);
                    const wchar_t* str = wstr;
                    mbstate_t state = {0};
                    size_t len = wcsrtombs(NULL, &str, 0, NULL);
                    if (len != (size_t)-1)
                    {
                        used += len;
                    }if (len != (size_t)-1)
                    {
                        if (precision_set && len > precision)
                        {
                            len = precision;
                        }

                        used += len;
                    }
                }
                else
                {
                    //  Byte string
                    uint_fast64_t so_far = 0;
                    for (const char* str = va_arg(args, const char*); *str && (!precision_set || so_far < precision); ++str, ++so_far)
                    {
                        used += 1;
                    }
                }
                break;

            case 'd':
            case 'i':
                //  Signed decimal integer
            {
                if (precision_set)
                {
                    flag_pad_leading_zeros = 0;
                }
                uintmax_t c;
                int is_negative = 0;
                switch (length)
                {
                case LENGTH_MOD_hh:
                {
                    const signed char v = (signed char)va_arg(args, int);
                    //  Add check for sign and put it in the buffer
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = (unsigned char)v;
                    }
                }
                    break;
                case LENGTH_MOD_h:
                {
                    const short v = (short) va_arg(args, int);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                case LENGTH_MOD_NONE:
                {
                    const int v = va_arg(args, int);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                case LENGTH_MOD_l:
                {
                    const long v = va_arg(args, long);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                case LENGTH_MOD_ll:
                {
                    const long long v = va_arg(args, long long);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                case LENGTH_MOD_j:
                case LENGTH_MOD_z:
                {
                    const intmax_t v = va_arg(args, intmax_t);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                case LENGTH_MOD_t:
                {
                    intptr_t v = va_arg(args, intptr_t);
                    if (v < 0)
                    {
                        is_negative = 1;
                        c = -v;
                    }
                    else
                    {
                        c = v;
                    }
                }
                    break;
                    //  This is fucked up
                case LENGTH_MOD_L:
                    goto end;
                }

                //  Put the absolute value in the buffer
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                while (c)
                {
                    assert(buffer_usage < reserved_buffer);
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_dec_and_shift(&c);
                }
                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (is_negative || flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (is_negative)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '-';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (is_negative || flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'o':
                //  Unsigned octal integer
            {
                if (precision_set)
                {
                    flag_pad_leading_zeros = 0;
                }
                uintmax_t c;
                switch (length)
                {
                case LENGTH_MOD_hh:
                    c = (unsigned char)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_h:
                    c = (unsigned short) va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_NONE:
                    c = (unsigned int)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_l:
                    c = (unsigned long)va_arg(args, unsigned long);
                    break;
                case LENGTH_MOD_ll:
                    c = va_arg(args, unsigned long long);
                    break;
                case LENGTH_MOD_j:
                case LENGTH_MOD_z:
                    c = va_arg(args, uintmax_t);
                case LENGTH_MOD_t:
                    c = (uintptr_t)va_arg(args, uintptr_t);
                    break;
                    //  This is fucked up
                case LENGTH_MOD_L:
                    goto end;
                }

                //  Put the absolute value in the buffer
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                while (c)
                {
                    assert(buffer_usage < reserved_buffer);
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_oct_and_shift(&c);
                }
                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (flag_alternative_conversion && buffer[(reserved_buffer - buffer_usage)] != '0')
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'x':
                //  Unsigned hex integer
            {
                if (precision_set)
                {
                    flag_pad_leading_zeros = 0;
                }
                uintmax_t c;
                switch (length)
                {
                case LENGTH_MOD_hh:
                    c = (unsigned char)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_h:
                    c = (unsigned short) va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_NONE:
                    c = (unsigned int)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_l:
                    c = (unsigned long)va_arg(args, unsigned long);
                    break;
                case LENGTH_MOD_ll:
                    c = va_arg(args, unsigned long long);
                    break;
                case LENGTH_MOD_j:
                case LENGTH_MOD_z:
                    c = va_arg(args, uintmax_t);
                case LENGTH_MOD_t:
                    c = (uintptr_t)va_arg(args, uintptr_t);
                    break;
                    //  This is fucked up
                case LENGTH_MOD_L:
                    goto end;
                }

                //  Put the absolute value in the buffer
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                const int was_zero = c != 0;
                while (c)
                {
                    assert(buffer_usage < reserved_buffer);
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_hex_and_shift(&c);
                }
                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (flag_alternative_conversion && !was_zero)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = 'x';
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'X':
                //  Unsigned hex integer
            {
                if (precision_set)
                {
                    flag_pad_leading_zeros = 0;
                }
                uintmax_t c;
                switch (length)
                {
                case LENGTH_MOD_hh:
                    c = (unsigned char)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_h:
                    c = (unsigned short) va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_NONE:
                    c = (unsigned int)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_l:
                    c = (unsigned long)va_arg(args, unsigned long);
                    break;
                case LENGTH_MOD_ll:
                    c = va_arg(args, unsigned long long);
                    break;
                case LENGTH_MOD_j:
                case LENGTH_MOD_z:
                    c = va_arg(args, uintmax_t);
                case LENGTH_MOD_t:
                    c = (uintptr_t)va_arg(args, uintptr_t);
                    break;
                    //  This is fucked up
                case LENGTH_MOD_L:
                    goto end;
                }

                //  Put the absolute value in the buffer
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                const int was_zero = c == 0;
                while (c)
                {
                    assert(buffer_usage < reserved_buffer);
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_HEX_and_shift(&c);
                }
                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (flag_alternative_conversion && !was_zero)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = 'X';
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'u':
                //  Unsigned decimal integer
            {
                if (precision_set)
                {
                    flag_pad_leading_zeros = 0;
                }
                uintmax_t c;
                switch (length)
                {
                case LENGTH_MOD_hh:
                    c = (unsigned char)va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_h:
                    c = (unsigned short) va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_NONE:
                    c = va_arg(args, unsigned int);
                    break;
                case LENGTH_MOD_l:
                    c = va_arg(args, unsigned long);
                    break;
                case LENGTH_MOD_ll:
                    c = va_arg(args, unsigned long long);
                    break;
                case LENGTH_MOD_j:
//                    c = va_arg(args, uintmax_t);
//                    break;
                case LENGTH_MOD_z:
//                    c = va_arg(args, size_t);
//                    break;
                case LENGTH_MOD_t:
                    c = va_arg(args, uintptr_t);
                    break;
                    //  This is fucked up
                case LENGTH_MOD_L:
                    goto end;
                }

                //  Put the absolute value in the buffer
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                while (c)
                {
                    assert(buffer_usage < reserved_buffer);
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_dec_and_shift(&c);
                }
                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer
                lin_jfree(support_allocator, buffer);
            }
                break;
            case 'p':
                //  Pointer
            {
                uintptr_t p = (uintptr_t)va_arg(args, void*);
                used += 1;
                used += 1;
                uint32_t i;
                for (i = 0; i < 16 && p; ++i)
                {
                    (void)unsigned_get_lsd_hex_and_shift(&p);
                }
                used += i;
            }
                break;
            case 'n':
                //  Output number of written parameters
            {
                void* const p = va_arg(args, void*);
                switch (length)
                {
                case LENGTH_MOD_hh:
                {
                    *((signed char*)p) = (signed char)used;
                }
                    break;
                case LENGTH_MOD_h:
                {
                    *((short *)p) = (short )used;
                }
                    break;
                case LENGTH_MOD_NONE:
                {
                    *((int*)p) = (int)used;
                }
                    break;
                case LENGTH_MOD_l:
                {
                    *((long*)p) = (long)used;
                }
                    break;
                case LENGTH_MOD_ll:
                {
                    *((long long*)p) = (long long)used;
                }
                    break;
                case LENGTH_MOD_j:
                case LENGTH_MOD_z:
                {
                    *((intmax_t*)p) = (intmax_t)used;
                }
                    break;
                case LENGTH_MOD_t:
                {
                    *((ptrdiff_t*)p) = (ptrdiff_t)used;
                }
                    break;
                case LENGTH_MOD_L:
                    goto end;
                }
            }
                break;

            case 'e':
            case 'E':
            {
                print_using_e_format:;
                intmax_t exponent;
                long double original;
                double base;
                int less_than_zero = 0;
                //  To convert it into form of a * 10^b, find a (base) and b (exponent)
                switch (length)
                {
                case LENGTH_MOD_NONE:
                case LENGTH_MOD_l:
                {
                    double v = va_arg(args, double);
                    if (isnan(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'e' || *ptr == 'g' || *ptr == 'E' || *ptr == 'G');
                        if (*ptr == 'e' || *ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'e' || *ptr == 'g' || *ptr == 'E' || *ptr == 'G');
                        if (*ptr == 'e' || *ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabs(v);
                    }
                    if (v == 0.0)
                    {
                        exponent = 0;
                        base = 0.0;
                        original = 0.0;
                    }
                    else
                    {
                        original = (long double)v;
                        double l10 = log10(v);
                        exponent = (intmax_t)l10;
#ifndef _WIN32
                        base = exp10(l10 - (double) exponent);
#else //!_WIN32
                        base = pow(10.0, l10 - (double) exponent);
#endif //_WIN32
                    }
                }
                    break;

                case LENGTH_MOD_L:
                {
                    long double v = va_arg(args, long double);
                    if (isnan(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'e' || *ptr == 'g' || *ptr == 'E' || *ptr == 'G');
                        if (*ptr == 'e' || *ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'e' || *ptr == 'g' || *ptr == 'E' || *ptr == 'G');
                        if (*ptr == 'e' || *ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabsl(v);
                    }

                    if (v == 0.0)
                    {
                        exponent = 0;
                        base = 0.0;
                        original = 0.0;
                    }
                    else
                    {
                        original = (long double)v;
                        long double l10 = log10l(v);
                        exponent = (intmax_t)l10;
#ifndef _WIN32
                        base = (double)exp10l(l10 - (long double) exponent);
#else //!_WIN32
                        base = (double)powl(10.0L, l10 - (long double) exponent);
#endif //_WIN32
                    }
                }
                    break;

                default:
                    goto end;
                }

                //  Reserve an auxiliary buffer for conversion
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                if (!precision_set)
                {
                    precision = 6;
                }

                //  Since we're printing the numbers into the buffer backwards, first put in the exponent
                int was_negative = exponent < 0;
                if (exponent < 0)
                {
                    exponent = -exponent + 1;
                    base *= 10;
                }
                for (uintmax_t e = exponent; e;)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_dec_and_shift(&e);
                }

                while (buffer_usage < 2)
                {
                    //  Exponent has less than two digits, fill it up with zeros
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Put the exponent sign and 'e'/'E'
                buffer[(reserved_buffer - ++buffer_usage)] = was_negative ? '-' : '+';
                assert(*ptr == 'e' || *ptr == 'g' || *ptr == 'E' || *ptr == 'G');
                buffer[(reserved_buffer - ++buffer_usage)] = *ptr == 'e' || *ptr == 'g' ? 'e' : 'E';  //  This is either 'e' or 'E'
                //  Now print the d.dddddd part
                uint32_t i;
                precision += 1;
#ifndef _WIN32
                double v = (double)(original * exp10l((long double)(was_negative ? exponent : -exponent)));
#else //!_WIN32
                double v = (double)(original * powl(10.0L, (long double)(was_negative ? exponent : -exponent)));
#endif //_WIN32
                for (i = 0; i < precision; ++i)
                {
                    char c = double_get_dig_and_shift(&v);
                    buffer[(reserved_buffer - buffer_usage - precision + i)] = c;//double_get_dig_and_shift(&base);
                }
                char remainder = (char)v;
                assert(remainder < 10);
                uint32_t j = i - 1;
                while (remainder)
                {
                    char current = (char)(buffer[(reserved_buffer - buffer_usage - precision + j)] + (remainder > 5));
                    if (current > '9')
                    {
                        remainder = 10;
                        current -= 10;
                    }
                    else
                    {
                        remainder = 0;
                    }
                    buffer[(reserved_buffer - buffer_usage - precision + j)] = current;
                    j -= 1;
                }
                buffer_usage += i;
                if (precision > 1 || flag_alternative_conversion)
                {
                    //  Move the last digit forward to make space for the decimal point
                    buffer[(reserved_buffer - (buffer_usage + 1))] = buffer[(reserved_buffer - buffer_usage)];
                    buffer[(reserved_buffer - buffer_usage)] = '.';
                    buffer_usage += 1;
                }
                //  The part missing is the sign and padding


                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (less_than_zero)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '-';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer

                //  Free the buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'a':
            case 'A':
            {
                int exponent;
                double base;
                int less_than_zero = 0;
                //  To convert it into form of a * 10^b, find a (base) and b (exponent)
                switch (length)
                {
                case LENGTH_MOD_NONE:
                case LENGTH_MOD_l:
                {
                    double v = va_arg(args, double);
                    if (v == 0.0)
                    {
                        exponent = 0;
                        base = 0.0;
                    }
                    else if (isnan(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'a' || *ptr == 'A');
                        if (*ptr == 'a')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = v;
                        if (*ptr == 'a')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabs(v);
                    }

                    base = frexp(v, &exponent);
                    exponent -= 1;
                    base = ldexp(base, 1);
                }
                    break;

                case LENGTH_MOD_L:
                {
                    long double v = va_arg(args, long double);
                    if (v == 0.0)
                    {
                        exponent = 0;
                        base = 0.0;
                    }
                    else if (isnan(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'a' || *ptr == 'A');
                        if (*ptr == 'a')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'a' || *ptr == 'A');
                        if (*ptr == 'a')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabsl(v);
                    }

                    long double tmp = frexpl(v, &exponent);
                    exponent -= 1;
                    base = (double)ldexpl(v, 1);
                }
                    break;

                default:
                    goto end;
                }

                //  Reserve an auxiliary buffer for conversion
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                if (!precision_set)
                {
                    precision = 6;
                }
                if (base == 0.0)
                {
                    exponent = 0;
                    precision = 0;
                }

                //  Since we're printing the numbers into the buffer backwards, first put in the exponent
                int was_negative = exponent < 0;
                if (exponent < 0)
                {
                    exponent = -exponent;
                }
                {
                    uintmax_t e = exponent;
                    do
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = unsigned_get_lsd_dec_and_shift(&e);
                    }
                    while (e);
                }
                //  Put the exponent sign and 'p'/'P'
                buffer[(reserved_buffer - ++buffer_usage)] = was_negative ? '-' : '+';
                assert(*ptr == 'a' || *ptr == 'A');
                buffer[(reserved_buffer - ++buffer_usage)] = *ptr == 'A' ? 'P' : 'p';  //  This is either 'e' or 'E'
                //  Now print the d.dddddd part
                uint32_t i;
                precision += 1;
                if (*ptr == 'a')
                {
                    for (i = 0; i < precision; ++i)
                    {
                        buffer[(reserved_buffer - buffer_usage - precision + i)] = double_get_hex_and_shift(&base);
                    }
                    const char last = double_get_hex_and_shift(&base);
                    static_assert('a' > '8');
                    if (last >= '8')
                    {
                        buffer[(reserved_buffer - buffer_usage - precision + i - 1)] += 1;
                    }
                }
                else
                {
                    for (i = 0; i < precision; ++i)
                    {
                        buffer[(reserved_buffer - buffer_usage - precision + i)] = double_get_HEX_and_shift(&base);
                    }
                    const char last = double_get_hex_and_shift(&base);
                    static_assert('a' > '8');
                    if (last >= '8')
                    {
                        buffer[(reserved_buffer - buffer_usage - precision + i - 1)] += 1;
                    }
                }
                buffer_usage += i;
                if (precision != 1 || flag_alternative_conversion)
                {
                    //  Move the last digit forward to make space for the decimal point
                    buffer[(reserved_buffer - (buffer_usage + 1))] = buffer[(reserved_buffer - buffer_usage)];
                    buffer[(reserved_buffer - buffer_usage)] = '.';
                    buffer_usage += 1;
                }
                //  Preappend the 0x/0X
                buffer[(reserved_buffer - ++buffer_usage)] = *ptr == 'A' ? 'X' : 'x';
                buffer[(reserved_buffer - ++buffer_usage)] = '0';


                //  The part missing is the sign and padding


                while (buffer_usage < precision)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (less_than_zero)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '-';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer

                //  Free the buffer
                lin_jfree(support_allocator, buffer);
            }
                break;

            case 'g':
            case 'G':
            {
                intmax_t P;
                if (!precision_set)
                {
                    P = 6;
                }
                else if (precision == 0)
                {
                    P = 1;
                }
                else
                {
                    P = precision;
                }
                intmax_t exponent;
                va_list copy;
                va_copy(copy, args);
                switch (length)
                {
                case LENGTH_MOD_NONE:
                case LENGTH_MOD_l:
                {
                    double v = va_arg(copy, double);
                    if (isnan(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'g' || *ptr == 'G');
                        if (*ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'g' || *ptr == 'G');
                        if (*ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        v = fabs(v);
                    }
                    if (v == 0.0)
                    {
                        exponent = 0;
                    }
                    else
                    {
                        double l10 = log10(v);
                        exponent = (intmax_t)l10;
                    }
                }
                    break;

                case LENGTH_MOD_L:
                {
                    long double v = va_arg(copy, long double);
                    if (isnan(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'g' || *ptr == 'G');
                        if (*ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'g' || *ptr == 'G');
                        if (*ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        v = fabsl(v);
                    }
                    if (v == 0.0)
                    {
                        exponent = 0;
                    }
                    else
                    {
                        long double l10 = log10l(v);
                        exponent = (intmax_t)l10;
                    }
                }
                    break;

                default:
                    goto end;
                }
                va_end(copy);
                int X = (int)exponent;
                precision_set = 1;
                clear_trailing_zeros = (flag_alternative_conversion == 0);
                if (P > X && X >= -4)
                {
                    precision = (int)(P - 1 - X);
                    goto print_using_f_format;
                }
                precision = (int)(P - 1);
                goto print_using_e_format;
            }
                break;

            case 'f':
            case 'F':
            {
                print_using_f_format:;
                double whole;
                double frac;
                int less_than_zero = 0;
                uint32_t pow_integer_part;
                //  To convert it into integer part
                switch (length)
                {
                case LENGTH_MOD_NONE:
                case LENGTH_MOD_l:
                {
                    double v = va_arg(args, double);
                    if (v == 0.0)
                    {
                        frac = 0.0;
                        whole = 0.0;
                        pow_integer_part = 0;
                    }
                    else if (isnan(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'f' || *ptr == 'g' || *ptr == 'F' || *ptr == 'G');
                        if (*ptr == 'f' || *ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = v;
                        assert(*ptr == 'f' || *ptr == 'g' || *ptr == 'F' || *ptr == 'G');
                        if (*ptr == 'f' || *ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabs(v);
                    }

                    frac = 10 * modf(v, &whole);
                    if (whole != 0.0)
                    {
                        pow_integer_part = (uint32_t)log10(whole);
                    }
                    else
                    {
                        pow_integer_part = 0;
                    }
                }
                    break;

                case LENGTH_MOD_L:
                {
                    long double v = va_arg(args, long double);
                    if (v == 0.0)
                    {
                        frac = 0.0;
                        whole = 0.0;
                        pow_integer_part = 0;
                    }
                    else if (isnan(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'f' || *ptr == 'g' || *ptr == 'F' || *ptr == 'G');
                        if (*ptr == 'f' || *ptr == 'g')
                        {
                            goto print_nan_small;
                        }
                        else
                        {
                            goto print_nan_big;
                        }
                    }
                    else if (isinf(v))
                    {
                        d_abnorm = (double)v;
                        assert(*ptr == 'f' || *ptr == 'g' || *ptr == 'F' || *ptr == 'G');
                        if (*ptr == 'f' || *ptr == 'g')
                        {
                            goto print_inf_small;
                        }
                        else
                        {
                            goto print_inf_big;
                        }
                    }
                    if (v < 0.0)
                    {
                        less_than_zero = 1;
                        v = fabsl(v);
                    }
                    long double tmp_w, tmp_f = 10 * modfl(v, &tmp_w);
                    whole = (double)tmp_w;
                    frac = (double)tmp_f;
                    if (whole != 0.0)
                    {
                        pow_integer_part = (uint32_t)log10l(tmp_w);
                    }
                    else
                    {
                        pow_integer_part = 0;
                    }
                }
                    break;

                default:
                    goto end;
                }

                //  Reserve an auxiliary buffer for conversion
                const size_t reserved_buffer = 64 < precision ? precision : 64;
                char* restrict buffer = lin_jalloc(support_allocator, reserved_buffer);
                if (!buffer)
                {
                    used = -1;
                    goto end;
                }
                size_t buffer_usage = 0;
                if (!precision_set)
                {
                    precision = 6;
                }

                //  Since we're printing the numbers into the buffer backwards, so the first part to be printed is the
                //  fractional part of the number stored in (frac). It should currently be multiplied by 10, so that
                //  the function double_get_dig_and_shift can get digits from it directly

                uint32_t i;
                {
                    double alt_frac = frac;
                    for (i = 0; i < precision
                                && (!clear_trailing_zeros || frac != 0.0)
                            ; ++i)
                    {
                        buffer[(reserved_buffer - buffer_usage - precision + i)] = double_get_dig_and_shift(&alt_frac);
                    }
                    //  Round last digit
                    char c = double_get_dig_and_shift(&alt_frac);
                    if (c >= '5')
                    {
                        uint32_t remainder = 1, j = 0;
                        while (j < precision && remainder)
                        {
                            c = buffer[(reserved_buffer - buffer_usage - (j + 1))];
                            if (c == '9')
                            {
                                remainder = 1;
                                buffer[(reserved_buffer - buffer_usage - (j + 1))] = '0';
                                j += 1;
                            }
                            else
                            {
                                remainder = 0;
                                buffer[(reserved_buffer - buffer_usage - (j + 1))] += 1;
                            }
                        }
                        if (remainder)
                        {
                            whole += 1.0;
                        }

                    }
                    buffer_usage += i;
                    if (((!clear_trailing_zeros || frac != 0.0) && precision) || flag_alternative_conversion)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = '.';
                    }
                }
                //  Put in the decimal point

                //  Now print the integer part has to be printed
                i = 0;
                for (i = 0; i < (pow_integer_part + 1)
                            && (i < 1 || whole != 0.0)
                        ; ++i)
                {
                    double alt_frac = whole / pow(10.0, (double)i);
                    double v = fmod(alt_frac, 10);
                    buffer[(reserved_buffer - ++buffer_usage)] = double_get_dig_and_shift(&v);
                }
                //  The part missing is the sign and padding
                assert(flag_pad_leading_zeros == 0 || (flag_left_justify == 0));
                while ((buffer_usage + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended)) < min_width && flag_pad_leading_zeros)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '0';
                }
                //  Add sign (or a space) if needed
                if (less_than_zero)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '-';
                }
                else if (flag_sign_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = '+';
                }
                else if (flag_space_pre_appended)
                {
                    buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                }

                //  If we pad with zeros, this should be it
                if (flag_pad_leading_zeros)
                {
                    assert(buffer_usage >= min_width);
                    assert(buffer_usage >= precision + (less_than_zero || flag_sign_pre_appended || flag_space_pre_appended));
                }
                //  If we don't justify left, we must pad now
                if (!flag_pad_leading_zeros && !(flag_left_justify))
                {
                    while (buffer_usage < min_width)
                    {
                        buffer[(reserved_buffer - ++buffer_usage)] = ' ';
                    }
                }
                assert(flag_left_justify == 1 || buffer_usage >= min_width);
                //  If we justify right, pad spaces to main buffer
                used += buffer_usage;
                if (flag_left_justify && buffer_usage < min_width)
                {
                    used += min_width - buffer_usage;
                }
                //  Release the temporary buffer

                //  Free the buffer
                lin_jfree(support_allocator, buffer);
            }
                break;
            }

            continue;
            {
                print_nan_small:;
                static const char SMALL_NAN_STR[3] = "nan";
                static_assert(sizeof(SMALL_NAN_STR) == 3);
                if ((flag_sign_pre_appended && !signbit(d_abnorm)) || signbit(d_abnorm))
                {
                    used += 1;
                }
                used += sizeof(SMALL_NAN_STR);
                continue;
            }
            {
                print_nan_big:;
                static const char BIG_NAN_STR[3] = "NAN";
                if ((flag_sign_pre_appended && !signbit(d_abnorm)) || signbit(d_abnorm))
                {
                    used += 1;
                }
                static_assert(sizeof(BIG_NAN_STR) == 3);
                used += sizeof(BIG_NAN_STR);
                continue;
            }
            {
                print_inf_small:;
                static const char SMALL_INF_STR[3] = "inf";
                if ((flag_sign_pre_appended && !signbit(d_abnorm)) || signbit(d_abnorm))
                {
                    used += 1;
                }
                static_assert(sizeof(SMALL_INF_STR) == 3);
                used += sizeof(SMALL_INF_STR);
                continue;
            }
            {
                print_inf_big:;
                static const char BIG_INF_STR[3] = "INF";
                if ((flag_sign_pre_appended && !signbit(d_abnorm)) || signbit(d_abnorm))
                {
                    used += 1;
                }
                static_assert(sizeof(BIG_INF_STR) == 3);
                used += sizeof(BIG_INF_STR);
                continue;
            }
        }
        else
        {
            used += 1;
        }
    }
    end:;
    //  Return the number of used characters
    return used;
}