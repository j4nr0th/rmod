#include "rmod.h"
#include "graph_parsing.h"

#include <stdio.h>

jallocator* G_JALLOCATOR;
linear_jallocator* G_LIN_JALLOCATOR;

static i32 error_hook(const char* thread_name, u32 stack_trace_count, const char*const* stack_trace, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param)
{
    fprintf(level > RMOD_ERROR_LEVEL_WARN ? stderr : stdout, "Thread (%s) sent %s from function %s in file %s at line %i: %s\n", thread_name,
            rmod_error_level_str(level), function, file, line, message);
    return 0;
}

static void print_xml_element(const xml_element * e, const u32 depth)
{
    for (u32 i = 0; i < depth; ++i)
        putchar('\t');
    printf("<%.*s (depth %u)", e->name.len, e->name.begin, e->depth);
    for (u32 i = 0; i < e->attrib_count; ++i)
    {
        printf(" %.*s=\"%.*s\"", e->attribute_names[i].len, e->attribute_names[i].begin, e->attribute_values[i].len, e->attribute_values[i].begin);
    }
    putchar('>');
    if (e->depth)
        putchar('\n');


    if (e->value.len)
    {
        if (e->depth)
            for (u32 i = 0; i < depth + 1; ++i)
                putchar('\t');
        printf("%.*s", e->value.len, e->value.begin);
        if (e->depth)
            putchar('\n');

    }

    for (u32 i = 0; i < e->child_count; ++i)
    {
        print_xml_element(e->children + i, depth + 1);
    }

    if (e->depth)
        for (u32 i = 0; i < depth; ++i)
            putchar('\t');
    printf("</%.*s>\n", e->name.len, e->name.begin);
}

int main()
{
    //  Main function of the RMOD program
    rmod_error_init_thread("master", RMOD_ERROR_LEVEL_NONE, 128, 128);
    RMOD_ENTER_FUNCTION;
    rmod_error_set_hook(error_hook, NULL);

    G_JALLOCATOR = jallocator_create((1 << 20), (1 << 19), 1);

    G_LIN_JALLOCATOR = lin_jallocator_create((1 << 20));


    element_type_id master_type_id;
    u32 n_types;
    rmod_element_type* p_types;

    FILE* example_file = fopen("../input/chain_dependence_test.xml", "r");
    if (!example_file)
    {
        RMOD_ERROR_CRIT("Could not open file, reason: %s", RMOD_ERRNO_MESSAGE);
    }
    fseek(example_file, 0, SEEK_END);
    size_t size = ftell(example_file) + 1;
    char* memory = calloc(size, 1);
    assert(memory);
    fseek(example_file, 0, SEEK_SET);
    size_t v = fread(memory, 1, size, example_file);
    assert(v);
    fclose(example_file);

    xml_element root;
    rmod_result res = rmod_parse_xml(size - 1, memory, &root);
    assert(res == RMOD_RESULT_SUCCESS);
    print_xml_element(&root, 0);

    res = rmod_convert_xml(&root, &n_types, &p_types);
    assert(res == RMOD_RESULT_SUCCESS);

    char* text;
    res = rmod_serialize_types(G_LIN_JALLOCATOR, n_types, p_types, &text);
    assert(res == RMOD_RESULT_SUCCESS);
    printf("Serialized types:\n %s\n", text);
    lin_jfree(G_LIN_JALLOCATOR, text);

    rmod_release_xml(&root);

    rmod_destroy_types(n_types, p_types);

    free(memory);

    {
        jallocator* const jallocator = G_JALLOCATOR;
        G_JALLOCATOR = NULL;
        jallocator_destroy(jallocator);
    }
    {
        linear_jallocator* const lin_jallocator = G_LIN_JALLOCATOR;
        G_LIN_JALLOCATOR = NULL;
        lin_jallocator_destroy(lin_jallocator);
    }
    RMOD_LEAVE_FUNCTION;
    rmod_error_cleanup_thread();
    return 0;
}
