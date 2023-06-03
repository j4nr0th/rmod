#include "rmod.h"
#include "graph_parsing.h"
#include "compile.h"
#include "program.h"
//#include "random/acorn.h"
#include "random/msws.h"
#include "simulation_run.h"
#include <inttypes.h>
#include <stdio.h>


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

static f64 rng_callback_function(void* param)
{
    return rmod_msws_rngf(param);
}

int main()
{
    //  Main function of the RMOD program
    rmod_error_init_thread("master", RMOD_ERROR_LEVEL_NONE, 128, 128);
    RMOD_ENTER_FUNCTION;
    rmod_error_set_hook(error_hook, NULL);

    G_JALLOCATOR = jallocator_create((1 << 20), (1 << 19), 2);

    G_LIN_JALLOCATOR = lin_jallocator_create((1 << 20));


    rmod_program program;
    rmod_result res = rmod_program_create("../input/chain_dependence_test.xml", &program);
    assert(res == RMOD_RESULT_SUCCESS);
    rmod_graph graph_a;
    char* text;
    res = rmod_serialize_types(G_LIN_JALLOCATOR, program.n_types, program.p_types, &text);
    assert(res == RMOD_RESULT_SUCCESS);
    printf("Serialized types before compilation:\n%s\n\n", text);
    lin_jfree(G_LIN_JALLOCATOR, text);
    res = rmod_compile(&program, &graph_a, "master", "main module");
    assert(res == RMOD_RESULT_SUCCESS);

    res = rmod_serialize_types(G_LIN_JALLOCATOR, program.n_types, program.p_types, &text);
    assert(res == RMOD_RESULT_SUCCESS);
    printf("Serialized types after compilation:\n%s\n\n", text);
    lin_jfree(G_LIN_JALLOCATOR, text);

    rmod_msws_state rng;
    rmod_msws_init(&rng);

    rmod_sim_result results = {};
    res = rmod_simulate_graph(&graph_a, 10, 1000000, &results, rng_callback_function, &rng);
    assert(res == RMOD_RESULT_SUCCESS);

    printf("Simulation results (took %g s for %lu runs, or %g s per run):\n\tAvg flow: %g\n\tAvg failures: %g\n\tAvg costs: %g\n", results.duration, results.sim_count, (f64)results.duration / (f64)results.sim_count, (f64)(results.total_flow)/(f64)results.sim_count, (f64)(results.total_failures)/(f64)results.sim_count, (f64)(results.total_costs)/(f64)results.sim_count);

    printf("Failures per component:\n");
    for (u32 i = 0; i < results.n_components; ++i)
    {
        printf("\t%u: %u\n", i, results.failures_per_component[i]);
    }
    jfree(results.failures_per_component);

    rmod_destroy_graph(&graph_a);
    rmod_program_delete(&program);

    assert(jallocator_verify(G_JALLOCATOR, NULL, NULL) == 0);
    uint_fast32_t forgotten_indices[128];
    uint_fast32_t r = jallocator_count_used_blocks(G_JALLOCATOR, 128, forgotten_indices);
    assert(r != -1);
    if (r > 0)
    {
        printf("Forgot to jfree %"PRIuFAST32" allocations:\n", r);
        for (u32 i = 0; i < r; ++i)
        {
            printf("\tAllocation of idx: %"PRIuFAST32"\n", forgotten_indices[i]);
        }
    }

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
