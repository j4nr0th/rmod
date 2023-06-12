//
// Created by jan on 4.6.2023.
//
#include "parsing_base.h"

#include <stdbool.h>
#include <libgen.h>
#include <unistd.h>


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

rmod_result rmod_release_xml(rmod_xml_element* root)
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


static void print_xml_element_to_file(const rmod_xml_element * e, const u32 depth, FILE* const file)
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
rmod_result rmod_serialize_xml(rmod_xml_element* root, FILE* f_out)
{
    RMOD_ENTER_FUNCTION;
    fprintf(f_out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE rmod [\n"
                   "        <!ELEMENT rmod (block|chain|include)*>\n"
                   "        <!ELEMENT block (name,mtbf,effect,failure,cost)>\n"
                   "        <!ELEMENT name (#PCDATA)>\n"
                   "        <!ELEMENT mtbf (#PCDATA)>\n"
                   "        <!ELEMENT effect (#PCDATA)>\n"
                   "        <!ELEMENT failure (#PCDATA)>\n"
                   "        <!ELEMENT cost (#PCDATA)>\n"
                   "        <!ELEMENT chain (name,first,last,element+)>\n"
                   "        <!ELEMENT first (#PCDATA)>\n"
                   "        <!ELEMENT last (#PCDATA)>\n"
                   "        <!ELEMENT element (type,label,(child|parent)*)>\n"
                   "        <!ATTLIST element\n"
                   "                etype (block|chain) #REQUIRED>\n"
                   "        <!ELEMENT type (#PCDATA)>\n"
                   "        <!ELEMENT label (#PCDATA)>\n"
                   "        <!ELEMENT child (#PCDATA)>\n"
                   "        <!ELEMENT parent (#PCDATA)>\n"
                   "        <!ELEMENT include (#PCDATA)>\n"
                   "        ]>");
    print_xml_element_to_file(root, 0, f_out);
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

rmod_result rmod_parse_xml(const rmod_memory_file* mem_file, rmod_xml_element* p_root)
{
    RMOD_ENTER_FUNCTION;
    const char* const xml = mem_file->ptr;
    const u64 len = mem_file->file_size;
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
    rmod_xml_element root = {};
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
    rmod_xml_element** current_stack = jalloc(stack_depth * sizeof(*current_stack));
    if (!current_stack)
    {
        RMOD_ERROR("Failed jalloc (%zu)", stack_depth * sizeof(*current_stack));
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    memset(current_stack, 0, stack_depth * sizeof(*current_stack));
    rmod_xml_element* current = &root;
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
                rmod_xml_element* const new_ptr = jrealloc(current->children, sizeof(*current->children) * new_capacity);
                if (!new_ptr)
                {
                    RMOD_ERROR("Failed jrealloc(%p, %zu)", current->children, sizeof(*current->children) * new_capacity);
                    goto free_fail;
                }
                memset(new_ptr + current->child_count, 0, sizeof(*new_ptr) * (new_capacity - current->child_capacity));
                current->child_capacity = new_capacity;
                current->children = new_ptr;
            }
            rmod_xml_element* new_child = current->children + (current->child_count++);
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
                rmod_xml_element** const new_ptr = jrealloc(current_stack, sizeof(*current_stack) * new_depth);
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

bool compare_string_segments(const string_segment* s1, const string_segment* s2)
{
    if (s1->len != s2->len)
        return false;
    return memcmp(s1->begin, s2->begin, s1->len) == 0;
}

bool compare_case_string_segment(u64 len, const char* str, const string_segment* segment)
{
    if (segment->len != len)
        return false;
    return strncasecmp(str, segment->begin, len) == 0;
}

bool compare_string_segment(u64 len, const char* str, const string_segment* segment)
{
    if (segment->len != len)
        return false;
    return memcmp(str, segment->begin, len) == 0;
}
