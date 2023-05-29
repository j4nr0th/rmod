//
// Created by jan on 27.5.2023.
//

#include "graph_parsing.h"

static const char WHITESPACE[] = {
        0x20,   //  this is ' '
        0x9,    //  this is '\t'
        0xD,    //  this is '\r'
        0xA     //  this is '\n'
};

static bool is_whitespace(c8 c)
{
    return !(c != WHITESPACE[0] && c != WHITESPACE[1] && c != WHITESPACE[2] && c != WHITESPACE[3]);
}

#define IS_IN_RANGE(v, btm, top) ((v) >= (btm) && (v) <= (top))

static inline bool is_name_start_char(c32 c)
{
    if (c == ':')
        return true;
    if (IS_IN_RANGE(c, 'A', 'Z'))
        return true;
    if (c == '_')
        return true;
    if (IS_IN_RANGE(c, 'a', 'z'))
        return true;
    if (IS_IN_RANGE(c, 0xC0, 0xD6))
        return true;
    if (IS_IN_RANGE(c, 0xD8, 0xF6))
        return  true;
    if (IS_IN_RANGE(c, 0xF8, 0x2FF))
        return true;
    if (IS_IN_RANGE(c, 0x370, 0x37D))
        return true;
    if (IS_IN_RANGE(c, 0x37F, 0x1FFF))
        return true;
    if (IS_IN_RANGE(c, 0x200C, 0x200D))
        return true;
    if (IS_IN_RANGE(c, 0x2070, 0x218F))
        return true;
    if (IS_IN_RANGE(c, 0x2C00, 0x2FEF))
        return true;
    if (IS_IN_RANGE(c, 0x3001, 0xD7FF))
        return true;
    if (IS_IN_RANGE(c, 0xF900, 0xFDCF))
        return true;
    if (IS_IN_RANGE(c, 0xFDF0, 0xFFFD))
        return true;
    if (IS_IN_RANGE(c, 0x10000, 0xEFFFF))
        return true;

    return false;
}

static inline bool is_name_char(c32 c)
{
    if (is_name_start_char(c))
        return true;
    if (c == '-')
        return true;
    if (c == '.')
        return true;
    if (IS_IN_RANGE(c, '0', '9'))
        return true;
    if (c == 0xB7)
        return true;
    if (IS_IN_RANGE(c, 0x0300, 0x036F))
        return true;
    if (IS_IN_RANGE(c, 0x203F, 0x2040))
        return true;

    return false;
}

static inline bool parse_utf8_to_utf32(u64 max_chars, const u8* ptr, u64* p_length, c32* p_char)
{
    assert(max_chars);
    c8 c = *ptr;
    if (!c)
    {
        return false;
    }
    c32 cp = 0;
    u32 len;
    if (c < 0x80)
    {
        //  ASCII character
        cp = c;
        len = 1;
    }
    else if ((c & 0xE0) == 0xC0 && max_chars > 1)
    {
        //  2 byte codepoint
        cp = ((c & 0x1F) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80 )
        {
            return false;
        }
        c &= 0x3F;
        cp |= c;
        len = 2;
    }
    else if ((c & 0xF0) == 0xE0 && max_chars > 2)
    {
        //  3 byte codepoint
        cp = ((c & 0x0F) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            return false;
        }
        c &= 0x3F;
        cp |= c;
        len = 3;
    }
    else if ((c & 0xF8) == 0xF0 && max_chars > 3)
    {
        //  4 byte codepoint -> at least U+10000, so don't support this
        cp = ((c & 0x07) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x2F;
        cp |= c;
        len = 4;
    }
    else
    {
        return false;
    }

    *p_char = cp;
    *p_length = len;
    return true;
}

static u32 count_new_lines(u32 length, const char* str)
{
    u64 i, c;
    for (i = 0, c = 0; i < length; ++i)
    {
        c += (str[i] == '\n');
    }
    return c;
}

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

rmod_result rmod_release_xml(xml_element* root)
{
    RMOD_ENTER_FUNCTION;
    jfree(root->attribute_values);
    jfree(root->attribute_names);
    for (u32 i = 0; i < root->child_count; ++i)
    {
        rmod_release_xml(root->children + i);
    }
    jfree(root->children);
    memset(root, 0, sizeof*root);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_WAS_NULL;
}

static bool parse_name_from_string(const u64 max_len, const char* const str, string_segment* out)
{
    const char* ptr = str;
    c32 c;
    u64 c_len;
    if (!parse_utf8_to_utf32(max_len - (ptr - str), (const u8*)ptr, &c_len, &c))
    {
        return false;
    }
    if (!is_name_start_char(c))
    {
        return false;
    }
    do
    {
        ptr += c_len;
        if (!parse_utf8_to_utf32(max_len - (ptr - str), (const u8*)ptr, &c_len, &c))
        {
            return false;
        }
    } while (is_name_char(c));

    out->begin = str;
    out->len = ptr - str;

    return true;
}

rmod_result rmod_parse_xml(const size_t len, const char* const xml, xml_element* p_root)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res;
    const char* pos;
    //  Parse the xml prologue (if present)
    if ((pos = strstr(xml, "<?xml")))
    {
        pos = strstr(pos, "?>");
        if (!pos)
        {
            RMOD_ERROR("Prologue to xml file has to have a form of \"<?xml\" ... \"?>\"");
            goto failed;
        }
        pos += 2;
    }
    else
    {
        pos = xml;
    }
    //  Search until reaching the root element
    c32 c;
    do
    {
        pos = strchr(pos, '<');
        if (!pos)
        {
            RMOD_ERROR("No root element was found");
            goto failed;
        }
        pos += 1;
        u64 c_len;
        if (!parse_utf8_to_utf32(len - (pos - xml), (const u8*) pos, &c_len, &c))
        {
            RMOD_ERROR("Invalid character encountered on line %u", count_new_lines(pos - xml, xml));
            goto failed;
        }
    } while(!is_name_start_char(c));

    //  We have now arrived at the root element
    xml_element root = {};
    //  Find the end of root's name
    if (!parse_name_from_string(len - (pos - xml), pos, &root.name))
    {
        RMOD_ERROR("Could not parse the name of the root tag");
        goto failed;
    }
    if (!(pos = strchr(pos, '>')))
    {
        RMOD_ERROR("Root element's start tag was not concluded");
        goto failed;
    }
    if (pos != root.name.begin + root.name.len)
    {
        RMOD_ERROR("Root tag should be only \"<rmod>\"");
        goto failed;
    }
    pos += 1;


    u32 stack_depth = 32;
    u32 stack_pos = 0;
    xml_element** current_stack = jalloc(stack_depth * sizeof(*current_stack));
    memset(current_stack, 0, stack_depth * sizeof(*current_stack));
    xml_element* current = &root;
    current_stack[0] = current;
    //  Perform descent down the element tree
    for (;;)
    {
        //  Skip any whitespace
        while (is_whitespace(*pos))
        {
            pos += 1;
        }
        current = current_stack[stack_pos];
        const char* new_pos = strchr(pos, '<');
        if (!new_pos)
        {
            RMOD_ERROR("Tag \"%.*s\" on line %u is unclosed", current->name.len, current->name.begin,
                       count_new_lines(current->name.begin - xml, xml));
            goto free_fail;
        }
        //  TODO: check space between pos and new_pos for data
        //  Skip any whitespace
        while (is_whitespace(*pos))
        {
            pos += 1;
        }
        //  We have some text before the next '<'
        string_segment val = {.begin = pos, .len = new_pos - pos};
        //  Trim space after the end of text
        while (val.len > 0 && is_whitespace(val.begin[val.len - 1]))
        {
            val.len -= 1;
        }
        current->value = val;

        pos = new_pos + 1;
        if (*pos == '/')
        {
            //  End tag
            if (strncmp(pos + 1, current->name.begin, current->name.len) != 0)
            {
                RMOD_ERROR("Tag \"%.*s\" on line %u was not properly closed", current->name.len, current->name.begin,
                           count_new_lines(current->name.begin - xml, xml));
                goto free_fail;
            }
            pos += 1 + current->name.len;
            if (*pos != '>')
            {
                RMOD_ERROR("Tag \"%.*s\" on line %u was not properly closed", current->name.len, current->name.begin,
                           count_new_lines(current->name.begin - xml, xml));
                goto free_fail;
            }
            pos += 1;
            if (stack_pos)
            {
                stack_pos -= 1;
            }
            else
            {
                goto done;
            }
        }
        else if (*pos == '!' && *(pos + 1) == '-' && *(pos + 2) == '-')
        {
            //  This is a comment
            pos += 3;
            new_pos = strstr(pos, "-->");
            if (!new_pos)
            {
                RMOD_ERROR("Comment on line %u was not concluded", count_new_lines(pos - xml, xml));
                goto free_fail;
            }
            pos = new_pos + 3;
        }
        else
        {
            //  New child tag
            string_segment name;
            if (!parse_name_from_string(len - (pos - xml), pos, &name))
            {
                RMOD_ERROR("Failed parsing tag name on line %u", count_new_lines(pos - xml, xml));
                goto free_fail;
            }
            pos += name.len;
            //  Add the child
            if (current->child_count == current->child_capacity)
            {
                const u64 new_capacity = current->child_capacity + 32;
                xml_element* const new_ptr = jrealloc(current->children, sizeof(*current->children) * new_capacity);
                if (!new_ptr)
                {
                    RMOD_ERROR("Failed jrealloc(%p, %zu)", current->children, sizeof(*current->children) * new_capacity);
                    goto free_fail;
                }
                memset(new_ptr + current->child_count, 0, sizeof(*new_ptr) * (new_capacity - current->child_capacity));
                current->child_capacity = new_capacity;
                current->children = new_ptr;
            }
            xml_element* new_child = current->children + (current->child_count++);
            new_child->name = name;


            while (is_whitespace(*pos))
            {
                pos += 1;
            }
            while (*pos != '>')
            {
                //  There are attributes to add
                string_segment attrib_name, attrib_val;
                if (!parse_name_from_string(len - (pos - xml), pos, &attrib_name))
                {
                    RMOD_ERROR("Failed parsing attribute name for block %.*s on line %u", name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    goto free_fail;
                }
                //  Next is the '=', which could be surrounded by whitespace
                pos += attrib_name.len;
                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
                if (*pos != '=')
                {
                    RMOD_ERROR("Failed parsing attribute %.*s for block %.*s on line %u: attribute name and value must be separated by '='", attrib_name.len, attrib_name.begin, name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    goto free_fail;
                }
                pos += 1;
                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
                if (*pos != '\'' && *pos != '\"')
                {
                    RMOD_ERROR("Failed parsing attribute %.*s for block %.*s on line %u: attribute value must be quoted", attrib_name.len, attrib_name.begin, name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    goto free_fail;
                }
                new_pos = strchr(pos + 1, *pos);
                if (!new_pos)
                {
                    RMOD_ERROR("Failed parsing attribute %.*s for block %.*s on line %u: attribute value quotes are not closed", attrib_name.len, attrib_name.begin, name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    goto free_fail;
                }
                attrib_val.begin = pos + 1;
                attrib_val.len = new_pos - pos - 1;
                pos = new_pos + 1;
                if (!is_whitespace(*pos) && *pos != '>')
                {
                    RMOD_ERROR("Failed parsing attribute %.*s for block %.*s on line %u: attributes should be separated by whitespace", attrib_name.len, attrib_name.begin, name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    goto free_fail;
                }
                if (new_child->attrib_count == new_child->attrib_capacity)
                {
                    const u32 new_capacity = new_child->attrib_capacity + 8;
                    string_segment* const new_ptr1 = jrealloc(new_child->attribute_names, sizeof(*new_ptr1) * new_capacity);
                    if (!new_ptr1)
                    {
                        RMOD_ERROR("Failed calling jrealloc(%p, %zu)", new_child->attribute_names, sizeof(*new_ptr1) * new_capacity);
                        goto free_fail;
                    }
                    memset(new_ptr1 + new_child->attrib_count, 0, sizeof(*new_ptr1) * (new_capacity - new_child->attrib_capacity));
                    new_child->attribute_names = new_ptr1;

                    string_segment* const new_ptr2 = jrealloc(new_child->attribute_values, sizeof(*new_ptr2) * new_capacity);
                    if (!new_ptr2)
                    {
                        RMOD_ERROR("Failed calling jrealloc(%p, %zu)", new_child->attribute_values, sizeof(*new_ptr2) * new_capacity);
                        goto free_fail;
                    }
                    memset(new_ptr2 + new_child->attrib_count, 0, sizeof(*new_ptr2) * (new_capacity - new_child->attrib_capacity));
                    new_child->attribute_values = new_ptr2;

                    new_child->attrib_capacity = new_capacity;
                }
                new_child->attribute_names[new_child->attrib_count] = attrib_name;
                new_child->attribute_values[new_child->attrib_count] = attrib_val;
                new_child->attrib_count += 1;

                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
            }
            pos += 1;


            //  Push the stack
            if (stack_pos == stack_depth)
            {
                const u64 new_depth = stack_depth + 32;
                xml_element** const new_ptr = jrealloc(current_stack, sizeof(*current_stack) * new_depth);
                if (!new_ptr)
                {
                    RMOD_ERROR("Failed jrealloc(%p, %zu)", current_stack, sizeof(*current_stack) * new_depth);
                    goto free_fail;
                }
                memset(new_ptr + stack_pos, 0, sizeof(*new_ptr) * (new_depth - stack_depth));
                stack_depth = new_depth;
                current_stack = new_ptr;
            }
            current_stack[++stack_pos] = new_child;
            if (current->child_count == 1)
            {
                assert(current->depth == 0);
                current->depth = 1;
                //  First child increases the depth of the tree
                for (u32 i = 0; i < stack_pos - 1; ++i)
                {
                    //  If a child now has an equal depth as parent, parent should have a greater depth
                    if (current_stack[stack_pos - 1 - i]->depth == current_stack[stack_pos - i - 2]->depth)
                    {
                        current_stack[stack_pos - 2 - i]->depth += 1;
                    }
                }
            }
        }
    }

done:
    assert(stack_pos == 0);
    jfree(current_stack);
    *p_root = root;
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

free_fail:
    rmod_release_xml(&root);
    jfree(current_stack);
failed:
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_BAD_XML;
}

static void print_xml_element_to_file(const xml_element * e, const u32 depth, FILE* const file)
{
    RMOD_ENTER_FUNCTION;
    for (u32 i = 0; i < depth; ++i)
        putc('\t', file);
    fprintf(file, "<%.*s", e->name.len, e->name.begin);
    for (u32 i = 0; i < e->attrib_count; ++i)
    {
        fprintf(file, " %.*s=\"%.*s\"", e->attribute_names[i].len, e->attribute_names[i].begin, e->attribute_values[i].len, e->attribute_values[i].begin);
    }
    putc('>', file);
    if (e->depth)
        putc('\n', file);


    if (e->value.len)
    {
        if (e->depth)
            for (u32 i = 0; i < depth + 1; ++i)
                putc('\t', file);
        fprintf(file, "%.*s", e->value.len, e->value.begin);
        if (e->depth)
            putc('\n', file);

    }

    for (u32 i = 0; i < e->child_count; ++i)
    {
        print_xml_element_to_file(e->children + i, depth + 1, file);
    }

    if (e->depth)
        for (u32 i = 0; i < depth; ++i)
            putc('\t', file);
    fprintf(file, "</%.*s>\n", e->name.len, e->name.begin);
    RMOD_LEAVE_FUNCTION;
}

rmod_result rmod_serialize_xml(xml_element* root, FILE* f_out)
{
    RMOD_ENTER_FUNCTION;
    fprintf(f_out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    print_xml_element_to_file(root, 0, f_out);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

static bool compare_xml_string(const u64 len, const char* str, const string_segment* xml)
{
    if (xml->len != len)
        return false;
    return memcmp(str, xml->begin, len) == 0;
}
static bool compare_case_xml_string(const u64 len, const char* str, const string_segment* xml)
{
    if (xml->len != len)
        return false;
    return strncasecmp(str, xml->begin, len) == 0;
}
static bool compare_string_segments(const string_segment* s1, const string_segment* s2)
{
    if (s1->len != s2->len)
        return false;
    return memcmp(s1->begin, s2->begin, s1->len) == 0;
}


#define COMPARE_XML_TO_LITERAL(literal, xml) compare_xml_string(sizeof(#literal) - 1, #literal, (xml))
#define COMPARE_CASE_XML_TO_LITERAL(literal, xml) compare_case_xml_string(sizeof(#literal) - 1, #literal, (xml))

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
        if (compare_xml_string(parts[0].label->len, parts[0].label->begin, parts[1].label))
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

rmod_result rmod_convert_xml(const xml_element* root, u32* pn_types, rmod_element_type** pp_types)
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
    if (!COMPARE_XML_TO_LITERAL(rmod, &root->name))
    {
        RMOD_ERROR("xml's root tag was not \"rmod\" but was \"%.*s\"", root->name.len, root->name.begin);
        res = RMOD_RESULT_BAD_XML;
        goto failed;
    }


    //  Parse children of the xml file which are 'block' and 'chain' nodes
    for (u32 i = 0; i < root->child_count; ++i)
    {
        const xml_element* e = root->children + i;
        if (COMPARE_XML_TO_LITERAL(block, &e->name))
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
            bool found_name = false, found_reliability = false, found_effect = false, found_failure = false;
            string_segment name_v = {};
            f32 reliability_v = 0.0f;
            f32 effect_v = 0.0f;
            rmod_failure_type failure_type_v = RMOD_FAILURE_TYPE_NONE;
            for (u32 j = 0; j < e->child_count; ++j)
            {
                const xml_element* child = e->children + j;
                if (COMPARE_XML_TO_LITERAL(name, &child->name))
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
                else if (COMPARE_XML_TO_LITERAL(reliability, &child->name))
                {
                    //  Reliability of the block
                    if (found_reliability)
                    {
                        RMOD_WARN("Duplicate \"reliability\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (child->value.len == 0)
                    {
                        RMOD_ERROR("Element \"reliability\" in the element \"block\" was empty");
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    char* end_pos;
                    reliability_v = strtof(child->value.begin, &end_pos);
                    if (end_pos != child->value.begin + child->value.len)
                    {
                        RMOD_ERROR("Value of \"reliability\" was given as \"%.*s\" in the element \"block\", which is not allowed (only a single float in range (0, 1] can be given)", child->value.len, child->value.begin);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    if (reliability_v <= 0.0f || reliability_v > 1.0f)
                    {
                        RMOD_ERROR("Value of \"reliability\" was given as \"%g\" in the element \"block\", which is not outside of the allowed range (0, 1]", reliability_v);
                        res = RMOD_RESULT_BAD_XML;
                        goto failed;
                    }
                    found_reliability = true;
                }
                else if (COMPARE_XML_TO_LITERAL(effect, &child->name))
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
                else if (COMPARE_XML_TO_LITERAL(failure, &child->name))
                {
                    //  Failure type of the block
                    if (found_failure)
                    {
                        RMOD_WARN("Duplicate \"failure\" element was found in the element in \"block\" and will be ignored");
                        continue;
                    }
                    if (COMPARE_CASE_XML_TO_LITERAL(normal, &child->value))
                    {
                        failure_type_v = RMOD_FAILURE_TYPE_ACCEPTABLE;
                    }
                    else if (COMPARE_CASE_XML_TO_LITERAL(critical, &child->value))
                    {
                        failure_type_v = RMOD_FAILURE_TYPE_CRITICAL;
                    }
                    else if (COMPARE_CASE_XML_TO_LITERAL(fatal, &child->value))
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
                else
                {
                    RMOD_WARN("Unknown element \"%.*s\" was found in the element \"block\" and will be ignored", e->name.len, e->name.begin);
                }
            }

            if (!found_name)
            {
                RMOD_ERROR("Element \"block\" did not include element \"name\"");
            }
            if (!found_reliability)
            {
                RMOD_ERROR("Element \"block\" did not include element \"reliability\"");
            }
            if (!found_effect)
            {
                RMOD_ERROR("Element \"block\" did not include element \"effect\"");
            }
            if (!found_failure)
            {
                RMOD_ERROR("Element \"block\" did not include element \"failure\"");
            }

            if (!found_name || !found_reliability || !found_effect || !found_failure)
            {
                RMOD_ERROR("Element \"block\" was not complete");
                res = RMOD_RESULT_BAD_XML;
                goto failed;
            }
            //  Check that name is not yet taken
            for (u32 j = 0; j < type_count; ++j)
            {
                if (compare_string_segments(&types[j].type.type_name, &name_v))
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
                        .type_header = { .type = RMOD_ELEMENT_TYPE_BLOCK, .type_name = name_v },
                        .effect = effect_v,
                        .failure_type = failure_type_v,
                        .reliability = reliability_v,
                        }
                    };
        }
        else if (COMPARE_XML_TO_LITERAL(chain, &e->name))
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
                const xml_element* child = e->children + j;
                if (COMPARE_XML_TO_LITERAL(name, &child->name))
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
                else if (COMPARE_XML_TO_LITERAL(first, &child->name))
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
                else if (COMPARE_XML_TO_LITERAL(last, &child->name))
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
                else if (COMPARE_XML_TO_LITERAL(element, &child->name))
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
                        if (COMPARE_XML_TO_LITERAL(etype, a_name))
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

                            if (COMPARE_XML_TO_LITERAL(block, a_valu))
                            {
                                type_v = RMOD_ELEMENT_TYPE_BLOCK;
                            }
                            else if (COMPARE_XML_TO_LITERAL(chain, a_valu))
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
                        const xml_element* component = child->children + k;
                        if (COMPARE_XML_TO_LITERAL(type, &component->name))
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
                                if (type->type.type != type_v)
                                {
                                    continue;
                                }
                                switch (type->type.type)
                                {
                                case RMOD_ELEMENT_TYPE_BLOCK:
                                    if (compare_string_segments(&type->type.type_name, &component->value))
                                    {
                                        type_id = l;
                                        found_type = true;
                                    }
                                    break;
                                case RMOD_ELEMENT_TYPE_CHAIN:
                                    if (compare_string_segments(&type->type.type_name, &component->value))
                                    {
                                        type_id = l;
                                        found_type = true;
                                    }
                                    break;
                                default:
                                    RMOD_ERROR("Invalid type enum value for type %s - %i", type->type.type_name, types->type);
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
                        else if (COMPARE_XML_TO_LITERAL(label, &component->name))
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
                        else if (COMPARE_XML_TO_LITERAL(parent, &component->name))
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
                        else if (COMPARE_XML_TO_LITERAL(child, &component->name))
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
                if (compare_string_segments(&types[j].type.type_name, name_ptr))
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
                    out->parents = nullptr;
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
                    out->children = nullptr;
                }

                out->label = *this->label;
                out->type_id = this->type_id;
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
                                            .type_header = { .type = RMOD_ELEMENT_TYPE_CHAIN, .type_name = *name_ptr },
                                            .chain_elements = chain_elements,
                                            .element_count = chain_element_count,
                                            .i_first = first_v,
                                            .i_last = last_v,
                                    }
                    };
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
        if (types[i].type.type == RMOD_ELEMENT_TYPE_CHAIN)
        {
            rmod_chain* const this = &types[i].chain;
            for (u32 j = 0; j < this->element_count; ++j)
            {
                jfree(this->chain_elements[j].parents);
                jfree(this->chain_elements[j].children);
            }
            jfree(this->chain_elements);
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
        switch (type->type.type)
        {
        case RMOD_ELEMENT_TYPE_CHAIN:
        {
            const rmod_chain* this = &type->chain;
            ADD_STRING_LITERAL("\t<chain>\n");

            ADD_STRING_LITERAL("\t\t<name>");
            ENSURE_BUFFER_SPACE(this->type_header.type_name.len);
            memcpy(buffer + usage, this->type_header.type_name.begin, this->type_header.type_name.len);
            usage += this->type_header.type_name.len;
            ADD_STRING_LITERAL("</name>\n");

            ADD_STRING_LITERAL("\t\t<first>");
            {
                const rmod_chain_element* first = this->chain_elements + this->i_first;
                ENSURE_BUFFER_SPACE(first->label.len);
                memcpy(buffer + usage, first->label.begin, first->label.len);
                usage += first->label.len;
            }
            ADD_STRING_LITERAL("</first>\n");
            ADD_STRING_LITERAL("\t\t<last>");
            {
                const rmod_chain_element* last = this->chain_elements + this->i_last;
                ENSURE_BUFFER_SPACE(last->label.len);
                memcpy(buffer + usage, last->label.begin, last->label.len);
                usage += last->label.len;
            }
            ADD_STRING_LITERAL("</last>\n");

            for (u32 i = 0; i < this->element_count; ++i)
            {
                const rmod_chain_element* e = this->chain_elements + i;
                ADD_STRING_LITERAL("\n\t\t<element etype=");
                ENSURE_BUFFER_SPACE(64);
                const char* type_string;
                switch (types[e->type_id].type.type)
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
                ADD_STRING_SEGMENT(types[e->type_id].type.type_name);
                ADD_STRING_LITERAL("</type>\n");

                ADD_STRING_LITERAL("\t\t\t<label>");
                ADD_STRING_SEGMENT(e->label);
                ADD_STRING_LITERAL("</label>\n");

                for (u32 j = 0; j < e->parent_count; ++j)
                {
                    ADD_STRING_LITERAL("\t\t\t<parent>");
                    ADD_STRING_SEGMENT((e + e->parents[j])->label);
                    ADD_STRING_LITERAL("</parent>\n");
                }

                for (u32 j = 0; j < e->child_count; ++j)
                {
                    ADD_STRING_LITERAL("\t\t\t<child>");
                    ADD_STRING_SEGMENT((e + e->children[j])->label);
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
            ADD_STRING_SEGMENT(this->type_header.type_name);
            ADD_STRING_LITERAL("</name>\n");

            ADD_STRING_LITERAL("\t\t<reliability>");
            ENSURE_BUFFER_SPACE(64);
            usage += sprintf(buffer + usage, "%f", this->reliability);
            ADD_STRING_LITERAL("</reliability>\n");

            ADD_STRING_LITERAL("\t\t<effect>");
            ENSURE_BUFFER_SPACE(64);
            usage += sprintf(buffer + usage, "%f", this->effect);
            ADD_STRING_LITERAL("</effect>\n");

            ADD_STRING_LITERAL("\t\t<failure>");
            ENSURE_BUFFER_SPACE(64);
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
        switch (types[i].type.type)
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
        }
            break;
        case RMOD_ELEMENT_TYPE_BLOCK:break;
        default:RMOD_WARN("Unknown type encountered for type \"%.*s\"", types->type.type_name.len, types->type.type_name.begin);
            break;
        }
    }
    jfree(types);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}
