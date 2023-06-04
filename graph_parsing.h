//
// Created by jan on 27.5.2023.
//

#ifndef RMOD_GRAPH_PARSING_H
#define RMOD_GRAPH_PARSING_H
#include "rmod.h"
#include "parsing_base.h"

typedef enum rmod_failure_type_enum rmod_failure_type;
enum rmod_failure_type_enum
{
    RMOD_FAILURE_TYPE_NONE,
    RMOD_FAILURE_TYPE_ACCEPTABLE,   //  Failure does not warrant maintenance by itself
    RMOD_FAILURE_TYPE_CRITICAL,     //  Needs immediate maintenance
    RMOD_FAILURE_TYPE_FATAL,        //  End simulation, can not recover from failure
    RMOD_FAILURE_TYPE_COUNT,
};

const char* rmod_failure_type_to_str(rmod_failure_type value);

typedef enum rmod_element_type_enum rmod_element_type_value;
enum rmod_element_type_enum
{
    RMOD_ELEMENT_TYPE_NONE,
    RMOD_ELEMENT_TYPE_BLOCK,
    RMOD_ELEMENT_TYPE_CHAIN,
    RMOD_ELEMENT_TYPE_COUNT,
};

const char* rmod_element_type_value_to_str(rmod_element_type_value value);

typedef struct rmod_element_type_header_struct rmod_element_type_header;
struct rmod_element_type_header_struct
{
    rmod_element_type_value type_value;
    string_segment type_name;
};

typedef struct rmod_block_struct rmod_block;
struct rmod_block_struct
{
    rmod_element_type_header header;
    f32 mtbf;
    f32 effect;
    f32 cost;
    rmod_failure_type failure_type;
};


typedef u32 element_type_id;

typedef struct rmod_chain_element_struct rmod_chain_element;
struct rmod_chain_element_struct
{
    element_type_id type_id;
    u32 id;
    string_segment label;
    u32 parent_count;
    ptrdiff_t* parents;
    u32 child_count;
    ptrdiff_t* children;
};

typedef struct rmod_chain_struct rmod_chain;
struct rmod_chain_struct
{
    rmod_element_type_header header;
    u32 element_count;
    u32 i_first;
    u32 i_last;
    rmod_chain_element* chain_elements;
    bool compiled;
};

typedef union rmod_element_type_union rmod_element_type;
union rmod_element_type_union
{
    rmod_element_type_header header;
    rmod_chain chain;
    rmod_block block;
};


rmod_result rmod_convert_xml(const rmod_xml_element* root, u32* p_file_count, u32* p_file_capacity, rmod_memory_file** pp_files, u32* pn_types, rmod_element_type** pp_types);

rmod_result rmod_serialize_types(linear_jallocator* allocator, u32 type_count, const rmod_element_type* types, char** p_out);

rmod_result rmod_destroy_types(u32 n_types, rmod_element_type* types);

#endif //RMOD_GRAPH_PARSING_H
