//
// Created by jan on 10.6.2023.
//

#ifndef RMOD_OPTION_PARSING_H
#define RMOD_OPTION_PARSING_H
#include "parsing_base.h"

typedef enum rmod_config_value_type_enum rmod_config_value_type;
enum rmod_config_value_type_enum
{
    RMOD_CFG_VALUE_NONE,        //  Not valid, just here to make sure to cause errors when type is not actually set
    RMOD_CFG_VALUE_UINT,
    RMOD_CFG_VALUE_INT,
    RMOD_CFG_VALUE_STR,
    RMOD_CFG_VALUE_REAL,
    RMOD_CFG_VALUE_CUSTOM,
};

typedef struct rmod_config_converter_uint_struct rmod_config_converter_uint;
struct rmod_config_converter_uint_struct
{
    rmod_config_value_type type;
    uintmax_t v_max;
    uintmax_t v_min;
    uintmax_t* p_out;
};

typedef struct rmod_config_converter_int_struct rmod_config_converter_int;
struct rmod_config_converter_int_struct
{
    rmod_config_value_type type;
    intmax_t v_max;
    intmax_t v_min;
    intmax_t* p_out;
};

typedef struct rmod_config_converter_str_structure rmod_config_converter_str;
struct rmod_config_converter_str_structure
{
    rmod_config_value_type type;
    string_segment* p_out;
};

typedef struct rmod_config_converter_real_structure rmod_config_converter_real;
struct rmod_config_converter_real_structure
{
    rmod_config_value_type type;
    double_t v_min;
    double_t v_max;
    double_t* p_out;
};

typedef struct rmod_config_converter_custom_struct rmod_config_converter_custom;
struct rmod_config_converter_custom_struct
{
    rmod_config_value_type type;
    bool(*converter_fn)(string_segment value, void* user_param, void* p_out);
    void* user_param;
    void* p_out;
};

typedef union rmod_config_converter_union rmod_config_converter;
union rmod_config_converter_union
{
    rmod_config_value_type type;
    rmod_config_converter_uint c_uint;
    rmod_config_converter_int c_int;
    rmod_config_converter_str c_str;
    rmod_config_converter_real c_real;
    rmod_config_converter_custom c_custom;
};


rmod_result convert_value(string_segment v, const rmod_config_converter* converter);



#endif //RMOD_OPTION_PARSING_H
