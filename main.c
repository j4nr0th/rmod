#include "rmod.h"
#include "graph_parsing.h"
#include "compile.h"
#include "program.h"
#include "random/msws.h"
#include "simulation_run.h"
#include "parsing_base.h"
#include <inttypes.h>
#include <stdio.h>
#include <libgen.h>
#include "config_parsing.h"


static i32 error_hook(const char* thread_name, u32 stack_trace_count, const char*const* stack_trace, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param)
{
    fprintf(level > RMOD_ERROR_LEVEL_WARN ? stderr : stdout, "Thread (%s) sent %s from function %s in file %s at line %i: %s\n", thread_name,
            rmod_error_level_str(level), function, file, line, message);
    return 0;
}

static void print_xml_element(const rmod_xml_element * e, const u32 depth)
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

typedef struct
{
    u32* p_out;
    u32 min_v;
    u32 max_v;
} uint32_conversion_params;

static bool convert_to_uint32(const string_segment value, void* const p_out)
{
    const uint32_conversion_params* const p_conv = p_out;
    char* end_pos = NULL;
    const uintmax_t v = strtoumax(value.begin, &end_pos, 10);
    if (end_pos != value.begin + value.len)
    {
        RMOD_ERROR("Failed conversion to uint32 due to invalid value: \"%.*s\"", value.len, value.begin);
        return false;
    }
    if (v < p_conv->min_v || v > p_conv->max_v)
    {
        RMOD_ERROR("Failed conversion to uint32 due to value \"%.*s\" outside of allowed range [%u, %u]", value.len, value.begin, p_conv->min_v, p_conv->max_v);
        return false;
    }
    *p_conv->p_out = v;

    return true;
}

int main(int argc, const char* argv[])
{
    //  For arguments, we accept only a single argument, which is an XML config file :)
    if (argc != 2)
    {
        //  print program usage
        printf("Usage: %s job_def\n\tjob_def: input xml file which contains description of the simulation to run\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    //  Main function of the RMOD program
    rmod_error_init_thread("master", RMOD_ERROR_LEVEL_NONE, 128, 128);
    RMOD_ENTER_FUNCTION;
    rmod_error_set_hook(error_hook, NULL);
    rmod_result res;
    G_JALLOCATOR = jallocator_create((1 << 20), (1 << 19), 2);

    G_LIN_JALLOCATOR = lin_jallocator_create((1 << 20));

    f64 sim_time;
    uintmax_t sim_reps;
    string_segment segment_filename;
    string_segment segment_chain;
    const rmod_config_entry config_children[] =  {
            {.name = "sim_time", .child_count = 0, .converter = {.c_real = {.p_out = &sim_time, .v_max = 100.0, .v_min = 1.0, .type = RMOD_CFG_VALUE_REAL }}},
            {.name = "sim_reps", .child_count = 0, .converter = {.c_uint = {.p_out = &sim_reps, .v_min = 1, .v_max = 10000000, .type = RMOD_CFG_VALUE_UINT}}},
            {.name = "filename", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_filename}}},
            {.name = "chain", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_chain}}}
    };
    const rmod_config_entry config_master =
            {
            .name = "config",
            .child_count = sizeof(config_children) / sizeof(*config_children),
            .child_array = config_children,
            };

    rmod_memory_file cfg_file;
    res = rmod_parse_configuration_file(argv[1], &config_master, &cfg_file);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Could not parse configuration file, reason: %s", rmod_result_str(res));
    }
    char* const chain_to_compile = lin_jalloc(G_LIN_JALLOCATOR, segment_chain.len + 1);
    if (!chain_to_compile)
    {
        RMOD_ERROR_CRIT("Failed allocating %zu bytes for chain to execute, reason: %s", segment_chain.len + 1, rmod_result_str(RMOD_RESULT_NOMEM));
    }
    memcpy(chain_to_compile, segment_chain.begin, segment_chain.len);
    chain_to_compile[segment_chain.len] = 0;
    char* const program_filename = lin_jalloc(G_LIN_JALLOCATOR, PATH_MAX);
    if (!program_filename)
    {
        RMOD_ERROR_CRIT("Failed allocating %zu bytes for program filename, reason: %s", PATH_MAX, rmod_result_str(RMOD_RESULT_NOMEM));
    }
    if (!realpath(argv[1], program_filename))
    {
        RMOD_ERROR_CRIT("Could not find full path to file \"%s\", reason: %s", argv[1], RMOD_ERRNO_MESSAGE);
    }
    {
        dirname(program_filename);
        const size_t len_argv1 = strlen(program_filename);
        snprintf(
                program_filename + len_argv1, PATH_MAX - len_argv1, "/%.*s", segment_filename.len,
                segment_filename.begin);
        rmod_unmap_file(&cfg_file);
    }


    rmod_program program;
    res = rmod_program_create(program_filename, &program);
    lin_jfree(G_LIN_JALLOCATOR, program_filename);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Could not create program to simulate, reason: %s", rmod_result_str(res));
    }
    rmod_graph graph_a;
    res = rmod_compile(&program, &graph_a, chain_to_compile, "main module");
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Failed compiling chain \"%s\", reason: %s", chain_to_compile, rmod_result_str(res));
    }
    lin_jfree(G_LIN_JALLOCATOR, chain_to_compile);


    rmod_msws_state rng;
    rmod_msws_init(&rng);

    rmod_sim_result results = {};
    res = rmod_simulate_graph(&graph_a, (f32)sim_time, (u32)sim_reps, &results, rng_callback_function, &rng);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Failed simulating graph [%s - %s], reason: %s", graph_a.module_name, graph_a.graph_type, rmod_result_str(res));
    }

    printf("Simulation results (took %g s for %lu runs, or %g s per run):\n\tAvg flow: %g\n\tAvg failures: %g\n\tAvg costs: %g\n", results.duration, results.sim_count, (f64)results.duration / (f64)results.sim_count, (f64)(results.total_flow)/(f64)results.sim_count, (f64)(results.total_failures)/(f64)results.sim_count, (f64)(results.total_costs)/(f64)results.sim_count);

    printf("Failures per component:\n");
    for (u32 i = 0; i < results.n_components; ++i)
    {
        printf("\t%u: %u\n", i, results.failures_per_component[i]);
    }
    jfree(results.failures_per_component);

    rmod_destroy_graph(&graph_a);
    rmod_program_delete(&program);
    int_fast32_t i_pool, i_chunk;
    if (jallocator_verify(G_JALLOCATOR, &i_pool, &i_chunk) != 0)
    {
        RMOD_ERROR("jallocator_verify fault with pool %"PRIiFAST32", chunk %"PRIiFAST32"", i_pool, i_chunk);
    }
    uint_fast32_t forgotten_indices[128];
    uint_fast32_t r = jallocator_count_used_blocks(G_JALLOCATOR, 128, forgotten_indices);
    assert(r != -1);
    if (r > 0)
    {
        RMOD_ERROR("Forgot to jfree %"PRIuFAST32" allocations:\n", r);
        for (u32 i = 0; i < r; ++i)
        {
            RMOD_ERROR("\tAllocation of idx: %"PRIuFAST32"\n", forgotten_indices[i]);
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
