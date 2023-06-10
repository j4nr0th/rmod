//
// Created by jan on 4.6.2023.
//

#ifndef RMOD_CONFIG_PARSING_H
#define RMOD_CONFIG_PARSING_H
#include "option_parsing.h"

typedef struct rmod_xml_config_entry_struct rmod_xml_config_entry;
struct rmod_xml_config_entry_struct
{
    const char* name;                       //  Name of the property/section
    const u32 child_count;                        //  Does it have children, and if so how many
    const rmod_xml_config_entry* child_array;   //  If child count is non-zero, this should point to array of children
    const rmod_config_converter converter;        //  Converter for the entry
};

rmod_result rmod_parse_xml_configuration(const rmod_xml_element* xml_root, const rmod_xml_config_entry* cfg_root);

rmod_result rmod_parse_configuration_file(const char* filename, const rmod_xml_config_entry* cfg_root, rmod_memory_file* pp_mem_file);

#endif //RMOD_CONFIG_PARSING_H
