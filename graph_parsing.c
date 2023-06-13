//
// Created by jan on 27.5.2023.
//

#include "graph_parsing.h"
#include "parsing_base.h"

#ifdef _WIN32
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

#else
#include <unistd.h>
#include <libgen.h>

#endif


const char* rmod_failure_type_to_str(rmod_failure_type value)
{
    if (value < 0 || value >= RMOD_FAILURE_TYPE_COUNT)
        return NULL;
    static const char* const name_array[RMOD_FAILURE_TYPE_COUNT] =
            {
                    [RMOD_FAILURE_TYPE_NONE] = "RMOD_FAILURE_TYPE_NONE",
                    [RMOD_FAILURE_TYPE_ACCEPTABLE] = "RMOD_FAILURE_TYPE_ACCEPTABLE",
                    [RMOD_FAILURE_TYPE_CRITICAL] = "RMOD_FAILURE_TYPE_CRITICAL",
                    [RMOD_FAILURE_TYPE_FATAL] = "RMOD_FAILURE_TYPE_FATAL",
            };
    return name_array[value];
}

const char* rmod_element_type_value_to_str(rmod_element_type_value value)
{
    if (value < 0 || value >= RMOD_ELEMENT_TYPE_COUNT)
        return NULL;
    static const char* const name_array[RMOD_ELEMENT_TYPE_COUNT] =
            {
                    [RMOD_ELEMENT_TYPE_NONE] = "RMOD_ELEMENT_TYPE_NONE",
                    [RMOD_ELEMENT_TYPE_BLOCK] = "RMOD_ELEMENT_TYPE_BLOCK",
                    [RMOD_ELEMENT_TYPE_CHAIN] = "RMOD_ELEMENT_TYPE_CHAIN",
            };
    return name_array[value];
}



typedef struct intermediate_element_struct intermediate_element;
struct intermediate_element_struct
{
    const string_segment* label;
    element_type_id type_id;
    u32 child_count;
    u32 child_capacity;
    const string_segment** child_names;
    u32 parent_count;
    u32 parent_capacity;
    const string_segment** parent_names;
};

static rmod_result confirm_unique_labels(const intermediate_element* parts, const u32 count)
{
    RMOD_ENTER_FUNCTION;
    if (count == 1)
    {
        RMOD_LEAVE_FUNCTION;
        return RMOD_RESULT_SUCCESS;
    }
    else if (count == 2)
    {
        if (compare_string_segment(parts[0].label->len, parts[0].label->begin, parts[1].label))
        {
            RMOD_ERROR("Label %.*s appears twice", parts[1].label->len, parts[1].label->begin);
            RMOD_LEAVE_FUNCTION;
            return RMOD_RESULT_BAD_XML;
        }
        RMOD_LEAVE_FUNCTION;
        return RMOD_RESULT_SUCCESS;
    }
    const u32 half = count / 2;

    rmod_result res = confirm_unique_labels(parts, half);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_LEAVE_FUNCTION;
        return res;
    }
    res = confirm_unique_labels(parts + half, count - half);
    RMOD_LEAVE_FUNCTION;
    return res;
}

static rmod_result check_flow(const string_segment* p_name, const rmod_chain_element* elements, const u32 element_id, const u32 depth, const u32 max_depth)
{
    if (depth > max_depth)
    {
        RMOD_ERROR("Cycle detected in chain \"%.*s\"", p_name->len, p_name->begin);
        return RMOD_RESULT_CYCLICAL_CHAIN;
    }

    const rmod_chain_element* e = elements + element_id;
    for (u32 i = 0; i < e->child_count; ++i)
    {
        rmod_result res = check_flow(p_name, elements, e->children[i], depth + 1, max_depth);
        if (res != RMOD_RESULT_SUCCESS)
        {
            return res;
        }
    }

    return RMOD_RESULT_SUCCESS;
}

//static rmod_result xml_insert_as_child(rmod_xml_element* p_dest, rmod_xml_element* p_src, u32 where)
//{
//    RMOD_ENTER_FUNCTION;
//    if (!COMPARE_XML_TO_LITERAL(rmod, &p_dest->name))
//    {
//        RMOD_ERROR("Destination was not a root of a rmod xml file");
//        RMOD_LEAVE_FUNCTION;
//        return RMOD_RESULT_BAD_XML;
//    }
//    if (!COMPARE_XML_TO_LITERAL(rmod, &p_src->name))
//    {
//        RMOD_ERROR("Source was not a root of a rmod xml file");
//        RMOD_LEAVE_FUNCTION;
//        return RMOD_RESULT_BAD_XML;
//    }
//    const u32 new_child_count = p_dest->child_count + p_src->child_count - 1;
//    const u32 new_attib_count = p_dest->attrib_count + p_src->attrib_count;
//    rmod_xml_element* const new_children = jrealloc(p_dest->children, sizeof*new_children * new_child_count);
//    if (!new_children)
//    {
//        RMOD_ERROR("Failed jrealloc(%p, %zu)", p_dest->children, sizeof*new_children * new_child_count);
//        RMOD_LEAVE_FUNCTION;
//        return RMOD_RESULT_NOMEM;
//    }
//    p_dest->children = new_children;
//    if (new_attib_count)
//    {
//        string_segment* const new_attrib_names = jrealloc(p_dest->attribute_names, sizeof*new_attrib_names * new_attib_count);
//        if (!new_attib_count)
//        {
//            RMOD_ERROR("Failed jrealloc(%p, %zu)", p_dest->attribute_names, sizeof*new_attrib_names * new_attib_count);
//            RMOD_LEAVE_FUNCTION;
//            return RMOD_RESULT_NOMEM;
//        }
//        p_dest->attribute_names = new_attrib_names;
//        string_segment* const new_attrib_values = jrealloc(p_dest->attribute_names, sizeof*new_attrib_values * new_attib_count);
//        if (!new_attib_count)
//        {
//            jfree(new_attrib_names);
//            RMOD_ERROR("Failed jrealloc(%p, %zu)", p_dest->attribute_names, sizeof*new_attrib_values * new_attib_count);
//            RMOD_LEAVE_FUNCTION;
//            return RMOD_RESULT_NOMEM;
//        }
//        p_dest->attribute_values = new_attrib_values;
//        memmove(new_attrib_names + (new_attib_count - p_dest->attrib_count), p_dest->attribute_names, sizeof(*new_attrib_names) * p_dest->attrib_count);
//        memcpy(new_attrib_names, p_src->attribute_names, sizeof(*new_attrib_names) * p_src->attrib_count);
//
//        memmove(new_attrib_values + (new_attib_count - p_dest->attrib_count), p_dest->attribute_values, sizeof(*new_attrib_values) * p_dest->attrib_count);
//        memcpy(new_attrib_values, p_src->attribute_values, sizeof(*new_attrib_values) * p_src->attrib_count);
//        p_dest->attrib_count = new_attib_count;
//    }
//    else
//    {
//        assert(p_dest->attribute_values == NULL);
//        p_dest->attribute_values = NULL;
//        assert(p_dest->attribute_names == NULL);
//        p_dest->attribute_names = NULL;
//    }
//
//    memmove(new_children + where + p_src->child_count, p_dest->children + where + 1, sizeof(*new_children) * p_dest->child_count - 1);
//    memcpy(new_children + where, p_src->children, sizeof(*new_children) * p_src->child_count);
//    p_dest->child_count = new_child_count;
//    jfree(p_src->children);
//    jfree(p_src->attribute_names);
//    jfree(p_src->attribute_values);
//    memset(p_src, 0, sizeof*p_src);
//    RMOD_LEAVE_FUNCTION;
//    return RMOD_RESULT_SUCCESS;
//}

rmod_result rmod_convert_xml(
        const rmod_xml_element* root, u32* p_file_count, u32* p_file_capacity, rmod_memory_file** pp_files, u32* pn_types,
        rmod_element_type** pp_types)
{
    RMOD_ENTER_FUNCTION;
    u32 type_count = 0;
    u32 type_capacity = 32;
    rmod_element_type* types = jalloc(sizeof*types * type_capacity);
    if (!types)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof*types * type_capacity);
        return RMOD_RESULT_NOMEM;
    }
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* base = lin_jalloc_get_current(G_LIN_JALLOCATOR);
    u32 part_count = 0;
    u32 part_capacity = 0;
    intermediate_element* element_buffer = NULL;
    if (!COMPARE_STRING_SEGMENT_TO_LITERAL(rmod, &root->name))
    {
        RMOD_ERROR("xml's root tag was not \"rmod\" but was \"%.*s\"", root->name.len, root->name.begin);
        res = RMOD_RESULT_BAD_XML;
        goto failed;
    }


    //  Parse children of the xml file which are 'block' and 'chain' nodes
    for (u32 i = 0; i < root->child_count; ++i)
    {
        const rmod_xml_element* e = root->children + i;
        if (COMPARE_STRING_SEGMENT_TO_LITERAL(block, &e->name))
        {
            //  Process attributes
            for (u32 k = 0; k < e->attrib_count; ++k)
            {
                const string_segment* a_name = e->attribute_names + k;
                const string_segment* a_valu = e->attribute_values + k;
                {
                    RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"element\" and will be ignored", a_name->len, a_name->begin);
                }

            }
            //  This is a block type definition
            bool found_name = false, found_mtbf = false, found_effect = false, found_failure = false, found_cost = false, found_mtbr = false;
            string_segment name_v = {};
            f32 mtbf_v = 0.0f;
            f32 effect_v = 0.0f;
            f32 cost_v = 0.0f;
            f32 mtbr_v = 0.0f;
            rmod_failure_type failure_type_v = RMOD_FAILURE_TYPE_NONE;
            for (u32 j = 0; j < e->child_count; ++j)
            {
                const rmod_xml_element* child = e->children + j;
                if (COMPARE_STRING_SEGMENT_TO_LITERAL(name, &child->name))
                {
                    //  Name of the block
                    if (found_name)
                    {
                        RMOD_WARN("Duplicate \"name\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"name\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    name_v = child->value;
                    found_name = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(mtbf, &child->name))
                {
                    //  Reliability of the block
                    if (found_mtbf)
                    {
                        RMOD_WARN("Duplicate \"mtbf\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"mtbf\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    char* end_pos;
                    mtbf_v = strtof(child->value.begin, &end_pos);
                    if (end_pos != child->value.begin + child->value.len)
                    {
                        RMOD_ERROR("Value of \"mtbf\" was given as \"%.*s\" in the element \"block\", which is not allowed (only a single positive float can be given)", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    if (mtbf_v < 0.0f)
                    {
                        RMOD_ERROR("Value of \"mtbf\" was given as \"%g\" in the element \"block\", which is must be positive", mtbf_v);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_mtbf = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(mtbr, &child->name))
                {
                    //  Reliability of the block
                    if (found_mtbr)
                    {
                        RMOD_WARN("Duplicate \"mtbr\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"mtbr\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    char* end_pos;
                    mtbr_v = strtof(child->value.begin, &end_pos);
                    if (end_pos != child->value.begin + child->value.len)
                    {
                        RMOD_ERROR("Value of \"mtbr\" was given as \"%.*s\" in the element \"block\", which is not allowed (only a single positive float can be given)", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    if (mtbr_v < 0.0f)
                    {
                        RMOD_ERROR("Value of \"mtbr\" was given as \"%g\" in the element \"block\", which is must be positive", mtbr_v);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_mtbr = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(effect, &child->name))
                {
                    //  Effect of the block
                    if (found_effect)
                    {
                        RMOD_WARN("Duplicate \"effect\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"effect\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    char* end_pos;
                    effect_v = strtof(child->value.begin, &end_pos);
                    if (end_pos != child->value.begin + child->value.len)
                    {
                        RMOD_ERROR("Value of \"effect\" was given as \"%.*s\" in the element \"block\", which is not allowed (only a single float in range (0, 1] can be given)", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    if (effect_v == 0.0f)
                    {
                        RMOD_ERROR("Value of \"effect\" was given as \"%g\" in the element \"block\", which is not allowed to be zero", effect_v);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_effect = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(failure, &child->name))
                {
                    //  Failure type of the block
                    if (found_failure)
                    {
                        RMOD_WARN("Duplicate \"failure\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (COMPARE_CASE_STRING_SEGMENT_TO_LITERAL(normal, &child->value))
                    {
                        failure_type_v = RMOD_FAILURE_TYPE_ACCEPTABLE;
                    }
                    else if (COMPARE_CASE_STRING_SEGMENT_TO_LITERAL(critical, &child->value))
                    {
                        failure_type_v = RMOD_FAILURE_TYPE_CRITICAL;
                    }
                    else if (COMPARE_CASE_STRING_SEGMENT_TO_LITERAL(fatal, &child->value))
                    {
                        failure_type_v = RMOD_FAILURE_TYPE_FATAL;
                    }
                    else
                    {
                        RMOD_ERROR("Value of \"failure\" element was given as \"%.*s\", which is not a valid value", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_failure = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(cost, &child->name))
                {
                    //  Failure type of the block
                    if (found_cost)
                    {
                        RMOD_WARN("Duplicate \"cost\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"cost\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    char* end_pos;
                    cost_v = strtof(child->value.begin, &end_pos);
                    if (end_pos != child->value.begin + child->value.len)
                    {
                        RMOD_ERROR("Value of \"cost\" was given as \"%.*s\" in the element \"block\", which is not allowed (only a single float can be given)", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_cost = true;
                }
                else
                {
                    RMOD_WARN("Unknown element \"%.*s\" was found in the element \"block\" and will be ignored", e->name.len, e->name.begin);
                }
            }

            if (!found_name)
            {
                RMOD_ERROR("Element \"block\" did not include element \"name\"");
            }
            if (!found_mtbf)
            {
                RMOD_ERROR("Element \"block\" did not include element \"mtbf\"");
            }
            if (!found_mtbr)
            {
                RMOD_ERROR("Element \"block\" did not include element \"mtbr\"");
            }
            if (!found_effect)
            {
                RMOD_ERROR("Element \"block\" did not include element \"effect\"");
            }
            if (!found_failure)
            {
                RMOD_ERROR("Element \"block\" did not include element \"failure\"");
            }
            if (!found_cost)
            {
                RMOD_ERROR("Element \"block\" did not include element \"cost\"");
            }

            if (!found_name || !found_mtbf || !found_effect || !found_failure || !found_cost || !found_mtbr)
            {
                RMOD_ERROR("Element \"block\" was not complete");
                res = RMOD_RESULT_BAD_XML;
                goto failed;
            }
            //  Check that name is not yet taken
            for (u32 j = 0; j < type_count; ++j)
            {
                if (compare_string_segments(&types[j].header.type_name, &name_v))
                {
                    RMOD_ERROR("Block/chain type \"%.*s\" was already defined", name_v.len, name_v.begin);
                    goto failed;
                }
            }
            //  Block can now be put in the type array
            if (type_count == type_capacity)
            {
                const u32 new_capacity = type_capacity + 32;
                rmod_element_type* const new_ptr = jrealloc(types, sizeof(*new_ptr) * new_capacity);
                if (!new_ptr)
                {
                    RMOD_ERROR("Failed jrealloc(%p, %zu)", types, sizeof(*new_ptr) * new_capacity);
                    res = RMOD_RESULT_NOMEM;
                    goto failed;
                }
                memset(new_ptr + type_count, 0, sizeof(*new_ptr) * (new_capacity - type_capacity));
                types = new_ptr;
                type_capacity = new_capacity;
            }
            types[type_count++] = (rmod_element_type)
                    {
                    .block =
                        {
                        .header = { .type_value = RMOD_ELEMENT_TYPE_BLOCK, .type_name = name_v },
                        .effect = effect_v,
                        .failure_type = failure_type_v,
                        .mtbf = mtbf_v,
                        .mtbr = mtbr_v,
                        .cost = cost_v,
                        }
                    };
        }
        else if (COMPARE_STRING_SEGMENT_TO_LITERAL(chain, &e->name))
        {
            assert(part_count == 0);
            //  This is a chain type definition
            bool found_name = false, found_first = false, found_last = false;
            const string_segment* name_ptr = NULL;
            const string_segment* str_first = NULL;
            const string_segment* str_last = NULL;
            memset(element_buffer, 0xCC, sizeof(*element_buffer) * part_capacity);
            u32 first_v = -1, last_v = -1;
            //  Process attributes
            for (u32 k = 0; k < e->attrib_count; ++k)
            {
                const string_segment* a_name = e->attribute_names + k;
                const string_segment* a_valu = e->attribute_values + k;
                {
                    RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"chain\" and will be ignored", a_name->len, a_name->begin);
                }

            }
            //  Process the children
            for (u32 j = 0; j < e->child_count; ++j)
            {
                const rmod_xml_element* child = e->children + j;
                if (COMPARE_STRING_SEGMENT_TO_LITERAL(name, &child->name))
                {
                    //  Name of the chain
                    if (found_name)
                    {
                        RMOD_WARN("Duplicate \"name\" element was found in the element in \"chain\" and will be ignored");
                        continue;
                    }
                    //  Process attributes
                    for (u32 k = 0; k < child->attrib_count; ++k)
                    {
                        const string_segment* a_name = child->attribute_names + k;
                        const string_segment* a_valu = child->attribute_values + k;
                        {
                            RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"name\" and will be ignored", a_name->len, a_name->begin);
                        }

                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"name\" in the element \"chain\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    name_ptr = &child->value;
                    found_name = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(first, &child->name))
                {
                    //  Name of the chain
                    if (found_first)
                    {
                        RMOD_WARN("Duplicate \"first\" element was found in the element in \"chain\" and will be ignored");
                        continue;
                    }
                    //  Process attributes
                    for (u32 k = 0; k < child->attrib_count; ++k)
                    {
                        const string_segment* a_name = child->attribute_names + k;
                        const string_segment* a_valu = child->attribute_values + k;
                        {
                            RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"element\" and will be ignored", a_name->len, a_name->begin);
                        }

                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"first\" in the element \"chain\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    str_first = &child->value;
                    found_first = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(last, &child->name))
                {
                    //  Last element in the chain
                    if (found_last)
                    {
                        RMOD_WARN("Duplicate \"first\" element was found in the element in \"chain\" and will be ignored");
                        continue;
                    }
                    //  Process attributes
                    for (u32 k = 0; k < child->attrib_count; ++k)
                    {
                        const string_segment* a_name = child->attribute_names + k;
                        const string_segment* a_valu = child->attribute_values + k;
                        {
                            RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"element\" and will be ignored", a_name->len, a_name->begin);
                        }

                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"last\" in the element \"chain\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    str_last = &child->value;
                    found_last = true;
                }
                else if (COMPARE_STRING_SEGMENT_TO_LITERAL(element, &child->name))
                {
                    //  One of chain's elements, contains:
                    //      - attribute "etype": "block" for a type of block and "chain" for type of chain
                    //      - element "label": unique identifier of the element within the chain
                    //      - optional element(s) "parent": identifies a parent of the block, can have many
                    //      - optional element(s) "child": identifies a child of the block, can have many
                    bool found_type = false, found_label = false, found_etype = false;
                    element_type_id type_id = -1;
                    const string_segment* label_v;
                    rmod_element_type_value type_v;
                    //  Process attributes
                    for (u32 k = 0; k < child->attrib_count; ++k)
                    {
                        const string_segment* a_name = child->attribute_names + k;
                        const string_segment* a_valu = child->attribute_values + k;
                        if (COMPARE_STRING_SEGMENT_TO_LITERAL(etype, a_name))
                        {
                            //  element's type can either be "block" or "chain"
                            if (found_etype)
                            {
                                RMOD_WARN("Duplicate \"etype\" attribute was found in the element in \"element\" and will be ignored");
                                continue;
                            }
                            if (a_valu->len == 0)
                            {
                                RMOD_ERROR("Attribute \"etype\" in the element \"element\" was empty");
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }

                            if (COMPARE_STRING_SEGMENT_TO_LITERAL(block, a_valu))
                            {
                                type_v = RMOD_ELEMENT_TYPE_BLOCK;
                            }
                            else if (COMPARE_STRING_SEGMENT_TO_LITERAL(chain, a_valu))
                            {
                                type_v = RMOD_ELEMENT_TYPE_CHAIN;
                            }
                            else
                            {
                                RMOD_ERROR("Attribute \"etype\" may only have values of \"block\" or \"chain\", but had the value of \"%.*s\"", a_valu->len, a_valu->begin);
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }

                            found_etype = true;
                        }
                        else
                        {
                            RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"element\" and will be ignored", a_name->len, a_name->begin);
                        }

                    }
                    if (!found_etype)
                    {
                        RMOD_ERROR("Element \"element\" did not specify it's type with an attribute \"etype\"");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    //  Get intermediate element
                    if (part_count == part_capacity)
                    {
                        const u64 new_capacity = part_capacity + 64;
                        intermediate_element* const new_ptr = jrealloc(element_buffer, sizeof*new_ptr * new_capacity);
                        if (!new_ptr)
                        {
                            RMOD_ERROR("Failed jrealloc(%p, %zu)", element_buffer, sizeof*new_ptr * new_capacity);
                            res = RMOD_RESULT_NOMEM;
                            goto failed;
                        }
                        memset(new_ptr + part_count, 0, sizeof*new_ptr * (new_capacity - part_capacity));
                        element_buffer = new_ptr;
                        part_capacity = new_capacity;
                    }

                    intermediate_element* const this = element_buffer + (part_count++);
                    memset(this, 0, sizeof(*this));
                    //  process children of "element"
                    for (u32 k = 0; k < child->child_count; ++k)
                    {
                        const rmod_xml_element* component = child->children + k;
                        if (COMPARE_STRING_SEGMENT_TO_LITERAL(type, &component->name))
                        {
                            //  Element's type
                            if (found_type)
                            {
                                RMOD_WARN("Duplicate \"type\" element was found in the element in \"element\" and will be ignored");
                                continue;
                            }
                            if (component->value.len == 0)
                            {
                                RMOD_ERROR("Element \"type\" in the element \"element\" was empty");
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }

                            for (u32 l = 0; l < type_count && !found_type; ++l)
                            {
                                const rmod_element_type* type = types + l;
                                if (type->header.type_value != type_v)
                                {
                                    continue;
                                }
                                switch (type->header.type_value)
                                {
                                case RMOD_ELEMENT_TYPE_BLOCK:
                                    if (compare_string_segments(&type->header.type_name, &component->value))
                                    {
                                        type_id = l;
                                        found_type = true;
                                    }
                                    break;
                                case RMOD_ELEMENT_TYPE_CHAIN:
                                    if (compare_string_segments(&type->header.type_name, &component->value))
                                    {
                                        type_id = l;
                                        found_type = true;
                                    }
                                    break;
                                default:
                                    RMOD_ERROR("Invalid type enum value for type %s - %i", type->header.type_name, types->header);
                                    res = RMOD_RESULT_BAD_XML;
                                    goto failed;
                                }
                            }

                            if (!found_type)
                            {
                                RMOD_ERROR("%s of type \"%.*s\" is not defined", type_v == RMOD_ELEMENT_TYPE_CHAIN ? "Chain" : "Block", component->value.len, component->value.begin);
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }
                        }
                        else if (COMPARE_STRING_SEGMENT_TO_LITERAL(label, &component->name))
                        {
                            //  Last element in the chain
                            if (found_label)
                            {
                                RMOD_WARN("Duplicate \"label\" element was found in the element in \"element\" and will be ignored");
                                continue;
                            }
                            //  Process attributes
                            for (u32 l = 0; l < component->attrib_count; ++k)
                            {
                                const string_segment* a_name = component->attribute_names + k;
                                const string_segment* a_valu = component->attribute_values + k;
                                {
                                    RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"label\" and will be ignored", a_name->len, a_name->begin);
                                }
                            }
                            if (component->value.len == 0)
                            {
                                RMOD_ERROR("Element \"label\" in the element \"element\" was empty");
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }

                            label_v = &component->value;
                            found_label = true;
                        }
                        else if (COMPARE_STRING_SEGMENT_TO_LITERAL(parent, &component->name))
                        {
                            //  A parent of the block

                            //  Process attributes
                            for (u32 l = 0; l < component->attrib_count; ++k)
                            {
                                const string_segment* a_name = component->attribute_names + k;
                                const string_segment* a_valu = component->attribute_values + k;
                                {
                                    RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"label\" and will be ignored", a_name->len, a_name->begin);
                                }
                            }
                            if (component->value.len == 0)
                            {
                                RMOD_ERROR("Element \"parent\" in the element \"element\" was empty");
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }
                            if (this->parent_count == this->parent_capacity)
                            {
                                const u64 new_capacity = this->parent_capacity + 64;
                                const string_segment** const new_ptr = jrealloc(this->parent_names, sizeof*new_ptr * new_capacity);
                                if (!new_ptr)
                                {
                                    RMOD_ERROR("Failed jrealloc(%p, %zu)", this->parent_names, sizeof*new_ptr * new_capacity);
                                    res = RMOD_RESULT_NOMEM;
                                    goto failed;
                                }
                                memset(new_ptr + this->parent_count, 0, sizeof*new_ptr * (new_capacity - this->parent_capacity));
                                this->parent_names = new_ptr;
                                this->parent_capacity = new_capacity;
                            }
                            this->parent_names[this->parent_count++] = &component->value;
                        }
                        else if (COMPARE_STRING_SEGMENT_TO_LITERAL(child, &component->name))
                        {
                            //  A child of the block

                            //  Process attributes
                            for (u32 l = 0; l < component->attrib_count; ++k)
                            {
                                const string_segment* a_name = component->attribute_names + k;
                                const string_segment* a_valu = component->attribute_values + k;
                                {
                                    RMOD_WARN("Unknown attribute \"%.*s\" was found in the element \"label\" and will be ignored", a_name->len, a_name->begin);
                                }
                            }
                            if (component->value.len == 0)
                            {
                                RMOD_ERROR("Element \"parent\" in the element \"element\" was empty");
                                res = RMOD_RESULT_BAD_XML;
                                goto failed;
                            }
                            if (this->child_count == this->child_capacity)
                            {
                                const u64 new_capacity = this->child_capacity + 64;
                                const string_segment** const new_ptr = jrealloc(this->child_names, sizeof*new_ptr * new_capacity);
                                if (!new_ptr)
                                {
                                    RMOD_ERROR("Failed jrealloc(%p, %zu)", this->child_names, sizeof*new_ptr * new_capacity);
                                    res = RMOD_RESULT_NOMEM;
                                    goto failed;
                                }
                                memset(new_ptr + this->child_count, 0, sizeof*new_ptr * (new_capacity - this->child_capacity));
                                this->child_names = new_ptr;
                                this->child_capacity = new_capacity;
                            }
                            this->child_names[this->child_count++] = &component->value;
                        }
                        else
                        {
                            RMOD_WARN("Unknown element \"%.*s\" was found in the element \"element\" and will be ignored", e->name.len, e->name.begin);
                        }
                    }
                    if (!found_label)
                    {
                        RMOD_ERROR("Element \"element\" did not contain element \"label\"");
                    }
                    if (!found_type)
                    {
                        RMOD_ERROR("Element \"element\" did not contain element \"type\"");
                    }

                    if (!found_type || !found_label)
                    {
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    this->label = label_v;
                    assert(type_id != -1);
                    this->type_id = type_id;
                }
                else
                {
                    RMOD_WARN("Unknown element \"%.*s\" was found in the element \"chain\" and will be ignored", e->name.len, e->name.begin);
                }
            }
            //  Now convert intermediate elements into proper form
            //  Make sure there's enough space for all of them

            for (u32 j = 0; j < type_count; ++j)
            {
                if (compare_string_segments(&types[j].header.type_name, name_ptr))
                {
                    RMOD_ERROR("Block/chain type \"%.*s\" was already defined", name_ptr->len, name_ptr->begin);
                    goto failed;
                }
            }
            const u32 chain_element_count = part_count;
            rmod_chain_element* const chain_elements = jalloc(sizeof*chain_elements * part_count);
            if (!chain_elements)
            {
                RMOD_ERROR("Failed jalloc(%zu)", sizeof*chain_elements * part_count);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }
            //  First make sure each part has a unique name
            if ((res = confirm_unique_labels(element_buffer, part_count)))
            {
                RMOD_ERROR("Not all elements in charin \"%.*s\" have unique labels", name_ptr->len, name_ptr->begin);
                goto failed;
            }

            if (!found_name)
            {
                RMOD_ERROR("There was not element \"name\" in element \"chain\"");
            }
            if (!found_first)
            {
                RMOD_ERROR("There was not element \"first\" in element \"chain\"");
            }
            if (!found_last)
            {
                RMOD_ERROR("There was not element \"last\" in element \"chain\"");
            }
            if (part_count == 0)
            {
                RMOD_ERROR("There was not element \"element\" in element \"chain\"");
            }

            if (!found_name || !found_first || !found_last || part_count == 0)
            {
                res = RMOD_RESULT_BAD_XML;
                goto failed;
            }

            //  Now begin conversion
            for (u32 j = 0; j < part_count; ++j)
            {
                rmod_chain_element* const out = chain_elements + j;
                const intermediate_element* const this = element_buffer + j;
                //  Check if it matches the first or last
                if (first_v == -1)
                {
                    if (compare_string_segments(str_first, this->label))
                    {
                        found_first = true;
                        first_v = j;
                    }
                }
                if (last_v == -1)
                {
                    if (compare_string_segments(str_last, this->label))
                    {
                        found_last = true;
                        last_v = j;
                    }
                }

                //  Process parents
                if (this->parent_count)
                {
                    ptrdiff_t* const parents = jalloc(sizeof*parents * this->parent_count);
                    if (!parents)
                    {
                        RMOD_ERROR("Failed jalloc(%zu)", sizeof*parents * this->parent_count);
                        jfree(chain_elements);
                        res = RMOD_RESULT_NOMEM;
                        goto failed;
                    }
                    //  Match parents
                    for (u32 k = 0; k < this->parent_count; ++k)
                    {
                        const string_segment* parent_name = this->parent_names[k];
                        u32 l;
                        for (l = 0; l < part_count; ++l)
                        {
                            if (compare_string_segments(parent_name, element_buffer[l].label))
                            {
                                parents[k] = l;
                                break;
                            }
                        }
                        if (l == part_count)
                        {
                            RMOD_ERROR("Element with label \"%.*s\" had a parent \"%.*s\", which was not found", this->label->len, this->label->begin, parent_name->len, parent_name->begin);
                            res = RMOD_RESULT_BAD_XML;
                            jfree(chain_elements);
                            goto failed;
                        }
                    }
                    out->parents = parents;
                    out->parent_count = this->parent_count;
                }
                else
                {
                    if (j != first_v)
                    {
                        RMOD_ERROR("Element with label \"%.*s\" specified no parents, but was not the beginning of the chain", this->label->len, this->label->begin);
                        res = RMOD_RESULT_BAD_XML;
                        jfree(chain_elements);
                        goto failed;
                    }
                    out->parent_count = 0;
                    out->parents = NULL;
                }

                //  Process children
                if (this->child_count)
                {
                    ptrdiff_t* const children = jalloc(sizeof*children * this->child_count);
                    if (!children)
                    {
                        RMOD_ERROR("Failed jalloc(%zu)", sizeof*children * this->child_count);
                        jfree(chain_elements);
                        res = RMOD_RESULT_NOMEM;
                        goto failed;
                    }
                    //  Match parents
                    for (u32 k = 0; k < this->child_count; ++k)
                    {
                        const string_segment* child_name = this->child_names[k];
                        u32 l;
                        for (l = 0; l < part_count; ++l)
                        {
                            if (compare_string_segments(child_name, element_buffer[l].label))
                            {
                                children[k] = l;
                                break;
                            }
                        }
                        if (l == part_count)
                        {
                            RMOD_ERROR("Element with label \"%.*s\" had a child \"%.*s\", which was not found", this->label->len, this->label->begin, child_name->len, child_name->begin);
                            res = RMOD_RESULT_BAD_XML;
                            jfree(chain_elements);
                            goto failed;
                        }
                    }
                    out->children = children;
                    out->child_count = this->child_count;
                }
                else
                {
                    if (j != last_v)
                    {
                        RMOD_ERROR("Element with label \"%.*s\" specified no children, but was not the end of the chain", this->label->len, this->label->begin);
                        jfree(chain_elements);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    out->child_count = 0;
                    out->children = NULL;
                }

                out->label = *this->label;
                out->type_id = this->type_id;
                out->id = j;
            }
            //  Ensure that there is correspondence between children and parents
            for (u32 j = 0; j < chain_element_count; ++j)
            {
                const rmod_chain_element* element = chain_elements + j;
                //  Check children
                for (u32 k = 0; k < element->child_count; ++k)
                {
                    bool was_found = false;
                    const rmod_chain_element* child_element = chain_elements + element->children[k];
                    for (u32 l = 0; l < child_element->parent_count; ++l)
                    {
                        if (child_element->parents[l] == j)
                        {
                            was_found = true;
                            break;
                        }
                    }
                    if (!was_found)
                    {
                        RMOD_ERROR("Element with label \"%.*s\" is not specified as parent of one of its children with label \"%.*s\"", element->label.len, element->label.begin, child_element->label.len, child_element->label.begin);
                        res = RMOD_RESULT_BAD_XML;
                        jfree(chain_elements);
                        goto failed;
                    }
                }
                //  Check parents
                for (u32 k = 0; k < element->parent_count; ++k)
                {
                    bool was_found = false;
                    const rmod_chain_element* parent_element = chain_elements + element->parents[k];
                    for (u32 l = 0; l < parent_element->child_count; ++l)
                    {
                        if (parent_element->children[l] == j)
                        {
                            was_found = true;
                            break;
                        }
                    }
                    if (!was_found)
                    {
                        RMOD_ERROR("Element with label \"%.*s\" is not specified as child of one of its parents with label \"%.*s\"", element->label.len, element->label.begin, parent_element->label.len, parent_element->label.begin);
                        res = RMOD_RESULT_BAD_XML;
                        jfree(chain_elements);
                        goto failed;
                    }
                }
            }
            //  Ensure the chain is non-cyclical
            if ((res = check_flow(name_ptr, chain_elements, first_v, 0, chain_element_count)) != RMOD_RESULT_SUCCESS)
            {
                RMOD_ERROR("Cycle detected in chain");
                jfree(chain_elements);
                goto failed;
            }

            //  Free the child and parent arrays
            for (u32 j = 0; j < part_count; ++j)
            {
                jfree(element_buffer[j].child_names);
                jfree(element_buffer[j].parent_names);
            }
            part_count = 0;
            
            //  Convert block relation (parent & child) into relative offsets. This allows for easier merging of chains
            u32 name_byte_count = 0;
            for (u32 j = 0; j < chain_element_count; ++j)
            {
                const rmod_chain_element* element = chain_elements + j;
                for (u32 k = 0; k < element->child_count; ++k)
                {
                    element->children[k] -= j;
                }
                for (u32 k = 0; k < element->parent_count; ++k)
                {
                    element->parents[k] -= j;
                }
                name_byte_count += element->label.len;
            }
            

            //  Chain was now parsed insert it into the type array
            if (type_count == type_capacity)
            {
                const u32 new_capacity = type_capacity + 32;
                rmod_element_type* const new_ptr = jrealloc(types, sizeof(*new_ptr) * new_capacity);
                if (!new_ptr)
                {
                    RMOD_ERROR("Failed jrealloc(%p, %zu)", types, sizeof(*new_ptr) * new_capacity);
                    jfree(chain_elements);
                    res = RMOD_RESULT_NOMEM;
                    goto failed;
                }
                memset(new_ptr + type_count, 0, sizeof(*new_ptr) * (new_capacity - type_capacity));
                types = new_ptr;
                type_capacity = new_capacity;
            }


            types[type_count++] = (rmod_element_type)
                    {
                            .chain =
                                    {
                                            .header = { .type_value = RMOD_ELEMENT_TYPE_CHAIN, .type_name = *name_ptr },
                                            .chain_elements = chain_elements,
                                            .element_count = chain_element_count,
                                            .i_first = first_v,
                                            .i_last = last_v,
                                            .compiled = false,
                                            .name_buffer = NULL,
                                            .name_bytes_total = name_byte_count,
                                    }
                    };
        }
        else if (COMPARE_STRING_SEGMENT_TO_LITERAL(include, &e->name))
        {
            //  Include tag: value is the name of the file which has to be converted into full path
            char* const name_buffer = lin_jalloc(G_LIN_JALLOCATOR, e->value.len + 1);
            if (!name_buffer)
            {
                RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, e->value.len + 1);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }
            memcpy(name_buffer, e->value.begin, e->value.len);
            name_buffer[e->value.len] = 0;

            char* buffer = lin_jalloc(G_LIN_JALLOCATOR, PATH_MAX);
            if (!buffer)
            {
                RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, PATH_MAX);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }
            char* working_dir = lin_jalloc(G_LIN_JALLOCATOR, PATH_MAX);
            if (!working_dir)
            {
                RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, PATH_MAX);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }

#ifndef _WIN32
            if (!realpath(name_buffer, buffer))
#else
            if (!GetFullPathName(name_buffer, MAX_PATH, buffer, NULL))
#endif
            {
                RMOD_ERROR("Could not find real path of file \"%s\", reason: %s", name_buffer, RMOD_ERRNO_MESSAGE);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }
            u32 j;
            for (j = 0; j < *p_file_count; ++j)
            {
                const rmod_memory_file* p_file = (*pp_files) + j;
                if (strcmp(p_file->name, buffer) == 0)
                {
                    break;
                }

            }

            if (j == *p_file_count)
            {
                if (!getcwd(working_dir, PATH_MAX))
                {
                    RMOD_ERROR("Failed getting the current working directory, reason: %s", RMOD_ERRNO_MESSAGE);
                    res = RMOD_RESULT_BAD_PATH;
                    goto failed;
                }
                //  Add new file to list and merge them
                rmod_memory_file mem_file;
                res = rmod_map_file_to_memory(buffer, &mem_file);
                if (res != RMOD_RESULT_SUCCESS)
                {
                    RMOD_ERROR("Could not map included file \"%s\" to memory", buffer);
                    goto failed;
                }
                rmod_xml_element included_root;
                res = rmod_parse_xml(&mem_file, &included_root);
                if (res != RMOD_RESULT_SUCCESS)
                {
                    rmod_unmap_file(&mem_file);
                    RMOD_ERROR("Failed parsing included file \"%s\"", buffer);
                    goto failed;
                }

                u32 n_new_types;
                rmod_element_type* p_new_types = NULL;
                char* new_wd = lin_jalloc(G_LIN_JALLOCATOR, PATH_MAX);
                if (!new_wd)
                {
                    rmod_release_xml(&included_root);
                    rmod_unmap_file(&mem_file);
                    RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, PATH_MAX);
                    res = RMOD_RESULT_NOMEM;
                    goto failed;
                }
                //  Literally the same size, so not checking this return value
                strncpy(new_wd, buffer, PATH_MAX);
                const int ret_1 = chdir(dirname(new_wd));
                lin_jfree(G_LIN_JALLOCATOR, new_wd);
                if (ret_1 < 0)
                {
                    RMOD_ERROR("could not change directory to location of \"%s\", reason: %s", new_wd, RMOD_ERRNO_MESSAGE);
                    rmod_release_xml(&included_root);
                    rmod_unmap_file(&mem_file);
                    res = RMOD_RESULT_BAD_PATH;
                    goto failed;
                }

                res = rmod_convert_xml(&included_root, p_file_count, p_file_capacity, pp_files, &n_new_types, &p_new_types);
                rmod_release_xml(&included_root);
                if (chdir(working_dir) < 0)
                {
                    jfree(p_new_types);
                    RMOD_ERROR("Could not change directory back to location of previous working directory \"%s\"", working_dir, RMOD_ERRNO_MESSAGE);
                    rmod_unmap_file(&mem_file);
                    res = RMOD_RESULT_BAD_PATH;
                    goto failed;
                }
                if (res != RMOD_RESULT_SUCCESS)
                {
                    rmod_unmap_file(&mem_file);
                    RMOD_ERROR("Failed converting included file \"%s\"", buffer);
                    goto failed;
                }

                //  Merge the files and types arrays
                if (type_count + n_new_types >= type_capacity)
                {
                    const u64 new_capacity = type_capacity + type_count;
                    rmod_element_type* const new_ptr = jrealloc(types, new_capacity * sizeof*types);
                    if (!new_ptr)
                    {
                        jfree(p_new_types);
                        rmod_unmap_file(&mem_file);
                        RMOD_ERROR("Failed jrealloc(%p, %zu)", types, new_capacity * sizeof*types);
                        res = RMOD_RESULT_NOMEM;
                        goto failed;
                    }
                    memset(new_ptr + type_count, 0, sizeof(*new_ptr) * (new_capacity - type_count));
                    type_capacity = new_capacity;
                    types = new_ptr;
                }
                if (*p_file_count >= *p_file_capacity)
                {
                    const u64 new_capacity = *p_file_capacity + 8;
                    rmod_memory_file* const new_ptr = jrealloc(*pp_files, new_capacity * sizeof(**pp_files));
                    if (!new_ptr)
                    {
                        jfree(p_new_types);
                        rmod_unmap_file(&mem_file);
                        RMOD_ERROR("Failed jrealloc(%p, %zu)", *pp_files, new_capacity * sizeof(**pp_files));
                        res = RMOD_RESULT_NOMEM;
                        goto failed;
                    }
                    memset(new_ptr + *p_file_count, 0, sizeof(*new_ptr) * (new_capacity - *p_file_count));
                    *p_file_capacity = new_capacity;
                    *pp_files = new_ptr;
                }

                //  For all elements in new chains update their type ids, since they need to be shifted
                for (u32 k = 0; k < n_new_types; ++k)
                {
                    const rmod_element_type* type = p_new_types + k;
                    if (type->header.type_value == RMOD_ELEMENT_TYPE_BLOCK)
                    {
                        continue;
                    }
                    const rmod_chain* chain = &type->chain;
                    for (u32 l = 0; l < chain->element_count; ++l)
                    {
                        rmod_chain_element* const element = chain->chain_elements + l;
                        element->type_id += type_count;
                    }
                }

                memcpy(types + type_count, p_new_types, sizeof(*types) * n_new_types);
                type_count += n_new_types;
                jfree(p_new_types);

                (*pp_files)[*p_file_count] = mem_file;
                *p_file_count += 1;
            }

            lin_jfree(G_LIN_JALLOCATOR, working_dir);
            lin_jfree(G_LIN_JALLOCATOR, buffer);
            lin_jfree(G_LIN_JALLOCATOR, name_buffer);
        }
        else
        {
            RMOD_WARN("Unknown element \"%.*s\" was found in the root \"rmod\" and will be ignored", e->name.len, e->name.begin);
        }
    }

    *pn_types = type_count;
    *pp_types = types;
    jfree(element_buffer);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

failed:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    for (u32 i = 0; i < part_count; ++i)
    {
        jfree(element_buffer[i].child_names);
        jfree(element_buffer[i].parent_names);
    }
    jfree(element_buffer);
    for (u32 i = 0; i < type_count; ++i)
    {
        if (types[i].header.type_value == RMOD_ELEMENT_TYPE_CHAIN)
        {
            rmod_chain* const this = &types[i].chain;
            for (u32 j = 0; j < this->element_count; ++j)
            {
                jfree(this->chain_elements[j].parents);
                jfree(this->chain_elements[j].children);
            }
            jfree(this->chain_elements);
            jfree(this->name_buffer);
        }
    }
    jfree(types);
    RMOD_LEAVE_FUNCTION;
    return res;
}


#define ENSURE_BUFFER_SPACE(needed) if (size <= usage + (needed)) { \
    size = usage + (needed) + 1024;                                 \
    char* const new_ptr = lin_jrealloc(allocator, buffer, size);    \
    if (!new_ptr)                                                   \
    {                                                               \
        RMOD_ERROR("Failed lin_jrealloc(%p, %p, %zu)", allocator, buffer, size); \
        res = RMOD_RESULT_NOMEM;\
        goto failure;\
    }                                                               \
    buffer = new_ptr;                                                                    \
}

#define ADD_STRING_LITERAL(str) ENSURE_BUFFER_SPACE(sizeof(str)); usage += sprintf(buffer + usage, "%s", str)

#define ADD_STRING_SEGMENT(seg) ENSURE_BUFFER_SPACE((seg).len); usage += sprintf(buffer + usage, "%.*s", (seg).len, (seg).begin)

rmod_result
rmod_serialize_types(linear_jallocator* allocator, const u32 type_count, const rmod_element_type* types, char** p_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_ERROR;
    u64 size = 1024;
    u64 usage = 0;
    char* buffer = lin_jalloc(allocator, size);
    if (!buffer)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", allocator, size);
        RMOD_LEAVE_FUNCTION;
        return RMOD_RESULT_NOMEM;
    }
    for (const rmod_element_type* p_type_pointer = types; p_type_pointer != types + type_count; ++p_type_pointer)
    {
        const rmod_element_type* type = p_type_pointer;
        switch (type->header.type_value)
        {
        case RMOD_ELEMENT_TYPE_CHAIN:
        {
            const rmod_chain* this = &type->chain;
            ADD_STRING_LITERAL("\t<chain>\n");

            ADD_STRING_LITERAL("\t\t<name>");
            ENSURE_BUFFER_SPACE(this->header.type_name.len)
            memcpy(buffer + usage, this->header.type_name.begin, this->header.type_name.len);
            usage += this->header.type_name.len;
            ADD_STRING_LITERAL("</name>\n");

            ADD_STRING_LITERAL("\t\t<first>");
            {
                const rmod_chain_element* first = this->chain_elements + this->i_first;
                if (first->label.len)
                {
                    ADD_STRING_SEGMENT(first->label);
                }
                else
                {
                    ADD_STRING_LITERAL("BLOCK-");
                    u32 len = snprintf(NULL, 0, "%03i", this->i_first);
                    ENSURE_BUFFER_SPACE(len)
                    usage += sprintf(buffer + usage, "%03i", this->i_first);
                }
            }
            ADD_STRING_LITERAL("</first>\n");
            ADD_STRING_LITERAL("\t\t<last>");
            {
                const rmod_chain_element* last = this->chain_elements + this->i_last;
                if (last->label.len)
                {
                    ADD_STRING_SEGMENT(last->label);
                }
                else
                {
                    ADD_STRING_LITERAL("BLOCK-");
                    u32 len = snprintf(NULL, 0, "%03i", this->i_last);
                    ENSURE_BUFFER_SPACE(len)
                    usage += sprintf(buffer + usage, "%03i", this->i_last);
                }
            }
            ADD_STRING_LITERAL("</last>\n");

            for (u32 i = 0; i < this->element_count; ++i)
            {
                const rmod_chain_element* e = this->chain_elements + i;
                ADD_STRING_LITERAL("\n\t\t<element etype=");
                ENSURE_BUFFER_SPACE(64)
                const char* type_string;
                switch (types[e->type_id].header.type_value)
                {
                case RMOD_ELEMENT_TYPE_CHAIN:
                    type_string = "\"chain\"";
                    break;
                case RMOD_ELEMENT_TYPE_BLOCK:
                    type_string = "\"block\"";
                    break;
                default:assert(0);
                    type_string = "\"?\"";
                    break;
                }
                usage += sprintf(buffer + usage, "%s", type_string);
                ADD_STRING_LITERAL(">\n\t\t\t<type>");
                ADD_STRING_SEGMENT(types[e->type_id].header.type_name);
                ADD_STRING_LITERAL("</type>\n");

                ADD_STRING_LITERAL("\t\t\t<label>");
                if (e->label.len)
                {
                    ADD_STRING_SEGMENT(e->label);
                }
                else
                {
                    ADD_STRING_LITERAL("BLOCK-");
                    u32 len = snprintf(NULL, 0, "%03i", i);
                    ENSURE_BUFFER_SPACE(len)
                    usage += sprintf(buffer + usage, "%03i", i);
                }
                ADD_STRING_LITERAL("</label>\n");

                for (u32 j = 0; j < e->parent_count; ++j)
                {
                    ADD_STRING_LITERAL("\t\t\t<parent>");
                    if ((e + e->parents[j])->label.len)
                    {
                        ADD_STRING_SEGMENT((e + e->parents[j])->label);
                    }
                    else
                    {
                        ADD_STRING_LITERAL("BLOCK-");
                        u32 len = snprintf(NULL, 0, "%03ji", i + e->parents[j]);
                        ENSURE_BUFFER_SPACE(len)
                        usage += sprintf(buffer + usage, "%03ji", i + e->parents[j]);
                    }
                    ADD_STRING_LITERAL("</parent>\n");
                }

                for (u32 j = 0; j < e->child_count; ++j)
                {
                    ADD_STRING_LITERAL("\t\t\t<child>");
                    if ((e + e->children[j])->label.len)
                    {
                        ADD_STRING_SEGMENT((e + e->children[j])->label);
                    }
                    else
                    {
                        ADD_STRING_LITERAL("BLOCK-");
                        u32 len = snprintf(NULL, 0, "%03ji", i + e->children[j]);
                        ENSURE_BUFFER_SPACE(len)
                        usage += sprintf(buffer + usage, "%03ji", i + e->children[j]);
                    }
                    ADD_STRING_LITERAL("</child>\n");
                }

                ADD_STRING_LITERAL("\t\t</element>\n");
            }
            ADD_STRING_LITERAL("\t</chain>\n");
        }
            break;

        case RMOD_ELEMENT_TYPE_BLOCK:
        {
            const rmod_block* this = &type->block;
            ADD_STRING_LITERAL("\t<block>\n");

            ADD_STRING_LITERAL("\t\t<name>");
            ADD_STRING_SEGMENT(this->header.type_name);
            ADD_STRING_LITERAL("</name>\n");

            ADD_STRING_LITERAL("\t\t<mtbf>");
            ENSURE_BUFFER_SPACE(64)
            usage += sprintf(buffer + usage, "%f", this->mtbf);
            ADD_STRING_LITERAL("</mtbf>\n");

            ADD_STRING_LITERAL("\t\t<effect>");
            ENSURE_BUFFER_SPACE(64)
            usage += sprintf(buffer + usage, "%f", this->effect);
            ADD_STRING_LITERAL("</effect>\n");

            ADD_STRING_LITERAL("\t\t<failure>");
            ENSURE_BUFFER_SPACE(64)
            switch (this->failure_type)
            {
            case RMOD_FAILURE_TYPE_ACCEPTABLE:
                usage += sprintf(buffer + usage, "%s", "normal");
                break;
            case RMOD_FAILURE_TYPE_CRITICAL:
                usage += sprintf(buffer + usage, "%s", "critical");
                break;
            case RMOD_FAILURE_TYPE_FATAL:
                usage += sprintf(buffer + usage, "%s", "fatal");
                break;
            default:assert(0);
                break;
            }
            ADD_STRING_LITERAL("</failure>\n");

            ADD_STRING_LITERAL("\t\t<cost>");
            ENSURE_BUFFER_SPACE(64)
            usage += sprintf(buffer + usage, "%f", this->cost);
            ADD_STRING_LITERAL("</cost>\n");

            ADD_STRING_LITERAL("\t</block>\n");
        }
            break;

        default:RMOD_ERROR("Type does not have a valid type value in its header");
            goto failure;
        }
    }

    *p_out = buffer;
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
failure:
    lin_jfree(allocator, buffer);
    RMOD_LEAVE_FUNCTION;
    return res;
}

rmod_result rmod_destroy_types(u32 n_types, rmod_element_type* types)
{
    RMOD_ENTER_FUNCTION;
    for (u32 i = 0; i < n_types; ++i)
    {
        switch (types[i].header.type_value)
        {
        case RMOD_ELEMENT_TYPE_CHAIN:
        {
            rmod_chain* const this = &types[i].chain;
            for (u32 j = 0; j < this->element_count; ++j)
            {
                jfree(this->chain_elements[j].parents);
                jfree(this->chain_elements[j].children);
            }
            jfree(this->chain_elements);
            jfree(this->name_buffer);
        }
            break;
        case RMOD_ELEMENT_TYPE_BLOCK:break;
        default:RMOD_WARN("Unknown type encountered for type \"%.*s\"", types->header.type_name.len, types->header.type_name.begin);
            break;
        }
    }
    jfree(types);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

