//
// Created by jan on 10.6.2023.
//

#ifndef RMOD_CLI_PARSING_H
#define RMOD_CLI_PARSING_H
#include "option_parsing.h"


typedef struct rmod_cli_config_entry_struct rmod_cli_config_entry;
struct rmod_cli_config_entry_struct
{
    bool found;                                     //  Set to true when found to make sure there are no duplicates
    bool needed;                                    //  Setting this to true mean option must be present
    const char* short_name;                         //  Short name of the property/section
    const char* long_name;                          //  Long name of the parameter
    const char* display_name;                       //  Name which is displayed when printing error messages
    const char* usage;                              //  Usage string printed for the option
    const rmod_config_converter converter;          //  Converter for the entry
};

rmod_result
rmod_parse_cli(u32 n_entries, rmod_cli_config_entry* entries, u32 n_values, const char* const values[]);

#endif //RMOD_CLI_PARSING_H
