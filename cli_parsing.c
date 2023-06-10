//
// Created by jan on 10.6.2023.
//
#include "cli_parsing.h"

rmod_result
rmod_parse_cli(u32 n_entries, rmod_cli_config_entry* entries, u32 n_values, const char* const values[])
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;

    for (u32 i = 0; i < n_values; ++i)
    {
        const char* specifier = values[i];
        if (specifier[0] != '-' || specifier[1] == 0)
        {
            RMOD_ERROR("Options must be specified in either short form (as \"-x\") or long form (as \"--x-option\"), instead \"%s\" was found", specifier);
            res = RMOD_RESULT_BAD_CLI;
            goto failed;
        }
        rmod_cli_config_entry* entry = NULL;
        if (specifier[1] == '-')
        {
            //  Long name
            for (u32 j = 0; j < n_entries; ++j)
            {
                if (strcmp(specifier + 2, entries[j].long_name) == 0)
                {
                    entry = entries + j;
                    break;
                }
            }
        }
        else
        {
            //  Short name
            for (u32 j = 0; j < n_entries; ++j)
            {
                if (strcmp(specifier + 1, entries[j].short_name) == 0)
                {
                    entry = entries + j;
                    break;
                }
            }
        }
        if (!entry)
        {
            RMOD_ERROR("Option specifier \"%s\" does not match any know options", specifier);
            res = RMOD_RESULT_BAD_CLI;
            goto failed;
        }
        if (entry->found)
        {
            RMOD_ERROR("Option \"%s\" was already provided", entry->display_name);
            res = RMOD_RESULT_BAD_CLI;
            goto failed;
        }
        const char* value;
        if ((i += 1) == n_values || ((value = values[i])[0] == '-'))
        {
            RMOD_ERROR("Option \"%s\" had no value given");
            res = RMOD_RESULT_BAD_CLI;
            goto failed;
        }
        if ((res = convert_value((string_segment){.begin = value, strlen(value)}, &entry->converter)))
        {
            RMOD_ERROR("Failed converting option \"%s\", reason: %s", entry->display_name, value);
            res = RMOD_RESULT_BAD_CLI;
            goto failed;
        }
        entry->found = true;
    }

    bool all_found = true;
    for (u32 i = 0; i < n_entries; ++i)
    {
        const rmod_cli_config_entry* entry = entries + i;
        if (entry->needed && !entry->found)
        {
            RMOD_ERROR("Option \"%s\" was not given but was specified as required", entry->display_name);
            all_found = false;
        }
    }
    if (!all_found)
    {
        res = RMOD_RESULT_BAD_CLI;
        goto failed;
    }
    
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

failed:
    return res;
}

