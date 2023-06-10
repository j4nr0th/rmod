#include "rmod.h"
#include "compile.h"
#include "program.h"
#include "random/msws.h"
#include "simulation_run.h"
#include "parsing_base.h"
#include <inttypes.h>
#include <stdio.h>
#include <libgen.h>
#include <float.h>
#include "config_parsing.h"


static i32 error_hook(const char* thread_name, u32 stack_trace_count, const char*const* stack_trace, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param)
{
    (void)param;
    (void)stack_trace;
    (void)stack_trace_count;
    fprintf(level > RMOD_ERROR_LEVEL_WARN ? stderr : stdout, "Thread (%s) sent %s from function %s in file %s at line %i: %s\n", thread_name,
            rmod_error_level_str(level), function, file, line, message);
    return 0;
}


static f64 rng_callback_function(void* param)
{
    return rmod_msws_rngf(param);
}

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

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
    printf("Initializing error handling and memory allocators\n");

    rmod_error_init_thread("master",
#ifndef NDEBUG
                           RMOD_ERROR_LEVEL_NONE,
#else
                            RMOD_ERROR_LEVEL_WARN,
#endif
                           128, 128);
    RMOD_ENTER_FUNCTION;
    rmod_error_set_hook(error_hook, NULL);
    rmod_result res;
    G_JALLOCATOR = jallocator_create((1 << 20), (1 << 19), 2);

    G_LIN_JALLOCATOR = lin_jallocator_create((1 << 20));

    f64 sim_time;
    uintmax_t sim_reps;
    string_segment segment_filename;
    string_segment segment_chain;
    uintmax_t thrd_count;
    f64 repair_limit;
    const rmod_config_entry config_children[] =  {
            {.name = "sim_time", .child_count = 0, .converter = {.c_real = {.p_out = &sim_time, .v_max = INFINITY, .v_min = FLT_EPSILON, .type = RMOD_CFG_VALUE_REAL }}},
            {.name = "sim_reps", .child_count = 0, .converter = {.c_uint = {.p_out = &sim_reps, .v_min = 1, .v_max = UINTMAX_MAX, .type = RMOD_CFG_VALUE_UINT}}},
            {.name = "filename", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_filename}}},
            {.name = "chain", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_chain}}},
            {.name = "threads", .child_count = 0, .converter = {.c_uint = {.type = RMOD_CFG_VALUE_UINT, .v_min = 0, .v_max = 256, .p_out = &thrd_count}}},
            {.name = "repair_limit", .child_count = 0, .converter = {.c_real = {.type = RMOD_CFG_VALUE_REAL, .v_min = 0.0, .v_max = INFINITY, .p_out = &repair_limit}}}
    };
    const rmod_config_entry config_master =
            {
            .name = "config",
            .child_count = sizeof(config_children) / sizeof(*config_children),
            .child_array = config_children,
            };

    rmod_memory_file cfg_file;
    printf("Parsing job description from file \"%s\"\n", argv[1]);
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
#ifndef _WIN32
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
#else
    {
        char* file_component;
        if (!GetFullPathNameA(argv[1], PATH_MAX, program_filename, &file_component))
        {
            //  IMPORTANT: ON WINDOWS ADD HANDLING OF WINDOWS NATIVE ERROR CODES GIVEN BY GetLastError()
            RMOD_ERROR_CRIT("Could not find full path to file \"%s\", reason: %s", argv[1], RMOD_ERRNO_MESSAGE);
        }
        snprintf(file_component, PATH_MAX - (uintptr_t)(file_component - program_filename), "%.*s", segment_filename.len, segment_filename.begin);
        rmod_unmap_file(&cfg_file);
    }
#endif


    rmod_program program;
    printf("Creating simulation program from file \"%s\"\n", program_filename);
    res = rmod_program_create(program_filename, &program);
    lin_jfree(G_LIN_JALLOCATOR, program_filename);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Could not create program to simulate, reason: %s", rmod_result_str(res));
    }
    rmod_graph graph_a;
    printf("Compiling program chain \"%s\" to graph\n", chain_to_compile);
    res = rmod_compile(&program, &graph_a, chain_to_compile, "main module");
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Failed compiling chain \"%s\", reason: %s", chain_to_compile, rmod_result_str(res));
    }
    lin_jfree(G_LIN_JALLOCATOR, chain_to_compile);


    rmod_msws_state rng;
    rmod_msws_init(&rng, 0, 0, 0, 0);

    rmod_sim_result results = {0};
    printf("Simulating graph built from chain \"%s\" containing %"PRIuFAST32" individual nodes\n", graph_a.graph_type, graph_a.node_count);
    if (thrd_count < 2)
    {
        res = rmod_simulate_graph(&graph_a, (f32) sim_time, (u32) sim_reps, &results, rng_callback_function, &rng, (f32)repair_limit);
        if (res != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR_CRIT("Failed simulating graph [%s - %s], reason: %s", graph_a.module_name, graph_a.graph_type, rmod_result_str(res));
        }
    }
    else
    {
        res = rmod_simulate_graph_mt(&graph_a, (f32) sim_time, (u32) sim_reps, &results, thrd_count, (f32)repair_limit);
        if (res != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR_CRIT("Failed simulating graph [%s - %s], reason: %s", graph_a.module_name, graph_a.graph_type, rmod_result_str(res));
        }
    }

#ifndef NDEBUG
    printf("Rng was called %"PRIu64" times\n", MSWS_TIMES_CALLED);
#endif //NDEBUG

    printf("Simulation results (took %g s for %lu runs, or %g s per run):\n\tAvg flow: %g\n\tAvg maintenance visits: %g\n\tAvg costs: %g\n", results.duration, results.sim_count, (f64)results.duration / (f64)results.sim_count, (f64)(results.total_flow)/(f64)results.sim_count/sim_time, (f64)(results.total_maintenance_visits) / (f64)results.sim_count, (f64)(results.total_costs) / (f64)results.sim_count);

    printf("Mean failures per component:\n");
    u32* const fails_per_type = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*fails_per_type) * graph_a.type_count);
    if (!fails_per_type)
    {
        RMOD_ERROR_CRIT("Failed allocating memory for array of fails per component type: failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*fails_per_type) * graph_a.type_count);
    }
    memset(fails_per_type, 0, sizeof(*fails_per_type) * graph_a.type_count);
    for (u32 i = 0; i < results.n_components; ++i)
    {
        const f64 avg_fails = (f64) results.failures_per_component[i] / (f64) sim_reps;
        printf("\t%u - %.*s: %lu (avg of %g, MTBF: %g)\n", i, graph_a.parent->chain_elements[i].label.len, graph_a.parent->chain_elements[i].label.begin, results.failures_per_component[i], avg_fails, (f64)sim_time / avg_fails);
        fails_per_type[graph_a.node_list[i].type_id] += results.failures_per_component[i];
    }
    jfree(results.failures_per_component);

    printf("Failures per type:\n");
    u64 total_failures = 0;
    for (u32 i = 0; i < graph_a.type_count; ++i)
    {
        printf("\t%s: %u\n", graph_a.type_list[i].name, fails_per_type[i]);
        total_failures += fails_per_type[i];
    }
    lin_jfree(G_LIN_JALLOCATOR, fails_per_type);
    printf("Total failures: %"PRIu64"\n", total_failures);

    printf("Cleaning up\n");
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
        uint_fast64_t max_alloc_size, total_allocated, max_usage, alloc_count;
        jallocator_statistics(jallocator, &max_alloc_size, &total_allocated, &max_usage, &alloc_count);
        printf("Jallocator statistics:\n\tBiggest allocation made: %"PRIuFAST64" bytes\n\tTotal allocated memory: %"PRIuFAST64" bytes\n\tMaximum usage: %"PRIuFAST64" bytes\n\tAllocations made: %"PRIuFAST64" allocations\n", max_alloc_size, total_allocated, max_usage, alloc_count);
        jallocator_destroy(jallocator);
    }
    {
        linear_jallocator* const lin_jallocator = G_LIN_JALLOCATOR;
        G_LIN_JALLOCATOR = NULL;
        printf("Peak lin_jalloc usage %"PRIuFAST64" bytes\n", lin_jallocator_destroy(lin_jallocator));
    }
    RMOD_LEAVE_FUNCTION;
    rmod_error_cleanup_thread();
    printf("Run complete\n");
    return 0;
}
