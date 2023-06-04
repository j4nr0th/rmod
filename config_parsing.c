//
// Created by jan on 4.6.2023.
//

#include "config_parsing.h"
#include "strings/formatted.h"
#include <inttypes.h>

static rmod_result report_child_mismatch(const rmod_xml_element* xml_element, const rmod_config_entry* cfg_element)
{
    size_t n_chars = sizeof("XML element \"\" had children [], but configuration specification wanted []") + xml_element->name.len;
    for (u32 i = 0; i < xml_element->child_count; ++i)
    {
        n_chars += xml_element->children[i].name.len + 2;// The +2 is there for the quotes
        n_chars += i == 0 ? 0 : 2; //   For the space and comma
    }
    if (xml_element->child_count == 0)
    {
        n_chars += 4;
    }
    for (u32 i = 0; i < cfg_element->child_count; ++i)
    {
        n_chars += strlen(cfg_element->child_array[i].name) + 2;// The +2 is there for the quotes
        n_chars += i == 0 ? 0 : 2; //   For the space and comma
    }
    if (cfg_element->child_count == 0)
    {
        n_chars += 4;
    }
    char* buffer = lin_jalloc(G_LIN_JALLOCATOR, n_chars + 1);
    if (!buffer)
    {
        RMOD_ERROR("Number of XML element's and config specification's child counts don't match (%u vs %u). Along with that failed lin_jalloc(%p, %zu)", xml_element->child_count, cfg_element->child_count, G_LIN_JALLOCATOR, n_chars + 1);
        return RMOD_RESULT_BAD_XML;
    }
    size_t pos = snprintf(buffer, n_chars, "XML element \"%.*s\" had children [", xml_element->name.len, xml_element->name.begin);
    if (xml_element->child_count == 0)
    {
        pos += snprintf(buffer + pos, n_chars - pos, "None");
    }
    else
    {
        pos += snprintf(buffer + pos, n_chars - pos, "\"%.*s\"", xml_element->children[0].name.len, xml_element->children[0].name.begin);
        for (u32 i = 1; i < xml_element->child_count; ++i)
        {
            pos += snprintf(buffer + pos, n_chars - pos, ", \"%.*s\"", xml_element->children[i].name.len, xml_element->children[i].name.begin);
        }
    }

    pos += snprintf(buffer + pos, n_chars - pos, "], but configuration specification wanted [");

    if (cfg_element->child_count == 0)
    {
        pos += snprintf(buffer + pos, n_chars - pos, "None");
    }
    else
    {
        pos += snprintf(buffer + pos, n_chars - pos, "\"%s\"", cfg_element->child_array[0].name);
        for (u32 i = 1; i < cfg_element->child_count; ++i)
        {
            pos += snprintf(buffer + pos, n_chars - pos, ", \"%s\"", cfg_element->child_array[i].name);
        }
    }

    snprintf(buffer + pos, n_chars - pos, "]");
    RMOD_ERROR("%s", buffer);
    lin_jfree(G_LIN_JALLOCATOR, buffer);
    return RMOD_RESULT_BAD_XML;
}

static inline rmod_result convert_value(const string_segment v, const rmod_config_converter* const converter)
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

static rmod_result recursive_parse_cfg(const rmod_xml_element* xml_element, const rmod_config_entry* cfg_element
                                       )
{
    if (cfg_element->child_count != xml_element->child_count)
    {
        return report_child_mismatch(xml_element, cfg_element);
    }
    if (cfg_element->child_count)
    {
        const u32 count = cfg_element->child_count;
        //  Used to determine xml <-> cfg correspondence (matched[i] = j means that xml[i] <-> cfg[j]
        u32* const matched = lin_jalloc(G_LIN_JALLOCATOR, sizeof*matched * count);
        if (!matched)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof*matched * count);
            return RMOD_RESULT_NOMEM;
        }
        memset(matched, -1, sizeof(*matched) * count);
        u32 match_count = 0;
        for (u32 i = 0; i < count; ++i)
        {
            const rmod_xml_element* xml_child = xml_element->children + i;
            for (u32 j = 0; j < count; ++j)
            {
                const rmod_config_entry* cfg_child = cfg_element->child_array + j;
                if (compare_string_segment(xml_child->name.len, cfg_child->name, &xml_child->name))
                {
                    if (matched[i] != -1)
                    {
                        RMOD_ERROR("Configuration element has more than one entry named \"%s\", which is not allowed", cfg_element->name);
                        return RMOD_RESULT_BAD_CFG;
                    }
                    matched[i] = j;
                    match_count += 1;
                }
            }
        }
        if (match_count != count)
        {
            return report_child_mismatch(xml_element, cfg_element);
        }
        for (u32 i = 0; i < count; ++i)
        {
            const u32 v = matched[i];
            for (u32 j = i + 1; j < count; ++j)
            {
                if (v == matched[j])
                {
                    RMOD_ERROR("Xml element \"%.*s\" is repeated more than once, which is not allowed", xml_element->children[i].name.len, xml_element->children[i].name.begin);
                    return RMOD_RESULT_BAD_XML;
                }
            }
        }

        rmod_result res;
        for (u32 i = 0; i < count; ++i)
        {
            if ((res = recursive_parse_cfg(xml_element->children + i, cfg_element->child_array + matched[i])) != RMOD_RESULT_SUCCESS)
            {
                lin_jfree(G_LIN_JALLOCATOR, matched);
                return res;
            }
        }

        lin_jfree(G_LIN_JALLOCATOR, matched);
    }
    else
    {
        rmod_result res;
        if ((res = convert_value(xml_element->value, &cfg_element->converter)) != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR("Conversion function of configuration component failed conversion for element \"%s\"", cfg_element->name);
            return res;
        }
    }


    return RMOD_RESULT_SUCCESS;
}

rmod_result rmod_parse_xml_configuration(const rmod_xml_element* xml_root, const rmod_config_entry* cfg_root)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res;
    if (strcmp(cfg_root->name, "config") != 0)
    {
        RMOD_ERROR("Configuration root has to be named config");
        res = RMOD_RESULT_BAD_CFG;
        goto end;
    }
    if (cfg_root->child_count == 0)
    {
        RMOD_ERROR("Configuration root must have children");
        res = RMOD_RESULT_BAD_CFG;
        goto end;
    }

    if (!COMPARE_STRING_SEGMENT_TO_LITERAL(config, &xml_root->name))
    {
        RMOD_ERROR("xml's root tag was not \"config\" but was \"%.*s\"", xml_root->name.len, xml_root->name.begin);
        res = RMOD_RESULT_BAD_XML;
        goto end;
    }


    res = recursive_parse_cfg(xml_root, cfg_root);

end:
    RMOD_LEAVE_FUNCTION;
    return res;
}

rmod_result rmod_parse_configuration_file(const char* filename, const rmod_config_entry* cfg_root, rmod_memory_file* pp_mem_file)
{
    RMOD_ENTER_FUNCTION;
    void* const base = lin_jalloc_get_current(G_LIN_JALLOCATOR);
    rmod_result res;

#ifndef _WIN32
    char* const real_name = lin_jalloc(G_LIN_JALLOCATOR, PATH_MAX);
    if (!real_name)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, PATH_MAX);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    if (!realpath(filename, real_name))
    {
        RMOD_ERROR("File path \"%s\" could not be resolved, reason: %s", filename, RMOD_ERRNO_MESSAGE);
        res = RMOD_RESULT_BAD_PATH;
        goto failed;
    }

#else
#error NOT IMPLEMENTED
#endif
    rmod_memory_file mem_file;
    if ((res = rmod_map_file_to_memory(real_name, &mem_file)) != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR("Could not map file to memory");
        goto failed;
    }
    lin_jfree(G_LIN_JALLOCATOR, real_name);
    rmod_xml_element root;
    if ((res = rmod_parse_xml(&mem_file, &root)) != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR("Could not parse xml");
        rmod_unmap_file(&mem_file);
        goto failed;
    }
    res = rmod_parse_xml_configuration(&root, cfg_root);
    rmod_release_xml(&root);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR("Could not parse configuration");
        rmod_unmap_file(&mem_file);
        goto failed;
    }
    *pp_mem_file = mem_file;


    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

failed:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}
