//
// Created by jan on 4.6.2023.
//

#ifndef RMOD_CONFIG_PARSING_H
#define RMOD_CONFIG_PARSING_H
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

typedef struct rmod_config_entry_struct rmod_config_entry;
struct rmod_config_entry_struct
{
    const char* name;                       //  Name of the property/section
    const u32 child_count;                        //  Does it have children, and if so how many
    const rmod_config_entry* child_array;   //  If child count is non-zero, this should point to array of children
    const rmod_config_converter converter;        //  Converter for the entry
};



rmod_result rmod_parse_xml_configuration(const rmod_xml_element* xml_root, const rmod_config_entry* cfg_root);

rmod_result rmod_parse_configuration_file(const char* filename, const rmod_config_entry* cfg_root, rmod_memory_file* pp_mem_file);

#endif //RMOD_CONFIG_PARSING_H
