//
// Created by jan on 4.6.2023.
//

#ifndef RMOD_PARSING_BASE_H
#define RMOD_PARSING_BASE_H

#include "../common/rmod.h"

typedef struct string_segment_struct string_segment;
typedef struct rmod_xml_element_struct rmod_xml_element;
struct string_segment_struct
{
    const char* begin;
    u32 len;
};
struct rmod_xml_element_struct
{
    u32 depth;
    string_segment name;
    u32 attrib_capacity;
    u32 attrib_count;
    string_segment* attribute_names;
    string_segment* attribute_values;
    u32 child_count;
    u32 child_capacity;
    rmod_xml_element* children;
    string_segment value;
};

rmod_result rmod_release_xml(rmod_xml_element* root);

rmod_result rmod_parse_xml(const rmod_memory_file* mem_file, rmod_xml_element* p_root);

rmod_result rmod_serialize_xml(rmod_xml_element* root, FILE* f_out);

bool compare_string_segment(u64 len, const char* str, const string_segment* segment);
bool compare_case_string_segment(u64 len, const char* str, const string_segment* segment);
bool compare_string_segments(const string_segment* s1, const string_segment* s2);


#define COMPARE_STRING_SEGMENT_TO_LITERAL(literal, xml) compare_string_segment(sizeof(#literal) - 1, #literal, (xml))
#define COMPARE_CASE_STRING_SEGMENT_TO_LITERAL(literal, xml) compare_case_string_segment(sizeof(#literal) - 1, #literal, (xml))


#endif //RMOD_PARSING_BASE_H
