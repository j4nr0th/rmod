//
// Created by jan on 10.6.2023.
//
#include "option_parsing.h"
#include <inttypes.h>


rmod_result convert_value(const string_segment v, const rmod_config_converter* const converter)
{
    RMOD_ENTER_FUNCTION;
    switch (converter->type)
    {
    case RMOD_CFG_VALUE_UINT:
    {
        const rmod_config_converter_uint* this = &converter->c_uint;
        //  Convert to uint
        char* end_p = NULL;
        uintmax_t val = strtoumax(v.begin, &end_p, 10);
        if (end_p != v.begin + v.len)
        {
            RMOD_ERROR("Failed conversion to uint due to invalid value \"%.*s\"", v.len, v.begin);
            goto failed;
        }
        if (val < this->v_min || val > this->v_max)
        {
            RMOD_ERROR("Converted value %"PRIuMAX" was outside of allowed range [%"PRIuMAX", %"PRIuMAX"]", val, this->v_min, this->v_max);
            goto failed;
        }
        *this->p_out = val;
    }
        break;

    case RMOD_CFG_VALUE_INT:
    {
        const rmod_config_converter_int* this = &converter->c_int;
        //  Convert to int
        char* end_p = NULL;
        intmax_t val = strtoimax(v.begin, &end_p, 10);
        if (end_p != v.begin + v.len)
        {
            RMOD_ERROR("Failed conversion to int due to invalid value \"%.*s\"", v.len, v.begin);
            goto failed;
        }
        if (val < this->v_min || val > this->v_max)
        {
            RMOD_ERROR("Converted value %"PRIiMAX" was outside of allowed range [%"PRIiMAX", %"PRIiMAX"]", val, this->v_min, this->v_max);
            goto failed;
        }
        *this->p_out = val;
    }
        break;

    case RMOD_CFG_VALUE_STR:
    {
        const rmod_config_converter_str* this = &converter->c_str;
        //  Just pass forward to the desired destination
        *this->p_out = v;
    }
        break;

    case RMOD_CFG_VALUE_REAL:
    {
        const rmod_config_converter_real * this = &converter->c_real;
        //  Convert to double
        char* end_p = NULL;
        double_t val = strtod(v.begin, &end_p);
        if (end_p != v.begin + v.len)
        {
            RMOD_ERROR("Failed conversion to double due to invalid value \"%.*s\"", v.len, v.begin);
            goto failed;
        }
        if (val < this->v_min || val > this->v_max)
        {
            RMOD_ERROR("Converted value %e was outside of allowed range [%e, %e]", val, this->v_min, this->v_max);
            goto failed;
        }
        *this->p_out = val;
    }
        break;

    case RMOD_CFG_VALUE_CUSTOM:
    {
        const rmod_config_converter_custom* this = &converter->c_custom;
        //  Pass to custom function
        if (!this->converter_fn(v, this->user_param, this->p_out))
        {
            RMOD_ERROR("Custom convertor function returned false, indicating failed conversion");
            goto failed;
        }
    }
        break;

    default:
    RMOD_ERROR("Config element converter has invalid type member");
        RMOD_LEAVE_FUNCTION;
        return RMOD_RESULT_STUPIDITY;

    }
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

    failed:
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_BAD_VALUE;
}
