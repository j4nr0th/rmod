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
#include <locale.h>
#include <unistd.h>
#include "config_parsing.h"
#include "fmt/sformatted.h"
#include "postprocessing.h"
#include "cli_parsing.h"

static i32 error_hook(const char* thread_name, u32 stack_trace_count, const char*const* stack_trace, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param)
{
    (void)param;
    (void)stack_trace;
    (void)stack_trace_count;
    switch (level)
    {
    case RMOD_ERROR_LEVEL_ERR:
        lin_eprintf(G_LIN_JALLOCATOR, "\x1b[31mError occurred on line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\x1b[0m\n", line, file, function, thread_name, message);
        break;
    case RMOD_ERROR_LEVEL_WARN:
        lin_eprintf(G_LIN_JALLOCATOR, "\x1b[33mWarning issued from line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\x1b[0m\n", line, file, function, thread_name, message);
        break;
    case RMOD_ERROR_LEVEL_INFO:
        printf("\x1b[37mInformation issued from line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\n\x1b[0m", line, file, function, thread_name, message);
        break;
    case RMOD_ERROR_LEVEL_NONE:
        printf("\x1b[35mMessage with incorrect severity of \"RMOD_ERROR_LEVEL_NONE\" issued from line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\x1b[0m\n", line, file, function, thread_name, message);
        break;
    case RMOD_ERROR_LEVEL_CRIT:
        printf("\x1b[35mMessage with incorrect severity of \"RMOD_ERROR_LEVEL_CRIT\" issued from line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\x1b[0m\n", line, file, function, thread_name, message);
        break;
    default:
        printf("\x1b[35mMessage with incorrect severity of ??? issued from line %i of file \"%s\" in function \"%s\", called by thread \"%s\": %s\x1b[0m\n", line, file, function, thread_name, message);
        break;
    }
    return 0;
}

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

int main(int argc, const char* argv[])
{
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

    G_JALLOCATOR = jallocator_create((1 << 20), (1 << 19), 2);
    if (!G_JALLOCATOR)
    {
        RMOD_ERROR_CRIT("Failed crating jallocator");
    }

    G_LIN_JALLOCATOR = lin_jallocator_create((4 << 20));
    if (!G_LIN_JALLOCATOR)
    {
        RMOD_ERROR_CRIT("Failed creating linear_jallocator");
    }
    rmod_error_set_hook(error_hook, NULL);
    rmod_result res;
    const char* arg_job_desc = NULL;
    string_segment out_file_name_segment = {0, 0};
    //  Process arguments
    bool args_wrong = false;
    rmod_cli_config_entry cli_cfg_entries[] =
            {
            [0] = {
                    .display_name = "output file",
                    .short_name = "o",
                    .long_name = "output",
                    .converter = {
                                    .c_str = { .type = RMOD_CFG_VALUE_STR, .p_out = &out_file_name_segment },
                                 },
                    .found = false,
                    .usage = "-o --output\t<file> output simulation results to <file> instead of to the stdout",
                    .needed = false,
                  },
            };
    const u32 n_cli_cfg_entries = sizeof(cli_cfg_entries) / sizeof(*cli_cfg_entries);
    if (argc < 2)
    {
        args_wrong = true;
    }
    else
    {
        if ((res = rmod_parse_cli(n_cli_cfg_entries, cli_cfg_entries, argc - 2, argv + 1)) != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR("Failed parsing command line arguments, reason: %s", rmod_result_str(res));
            args_wrong = true;
        }
        arg_job_desc = argv[argc - 1];
    }

    if (args_wrong)
    {
        //  print program usage
        printf("Usage: %s [options] job_def\n"
               "\toptions:\n", argv[0]);
        for (u32 i = 0; i < n_cli_cfg_entries; ++i)
        {
            printf("\t\t%s\n", cli_cfg_entries[i].usage);
        }
        printf("\tjob_def: input xml file which contains description of the simulation to run\n");
        exit(EXIT_SUCCESS);
    }

    f64 sim_time;
    uintmax_t sim_reps;
    string_segment segment_filename;
    string_segment segment_chain;
    uintmax_t thrd_count;
    f64 repair_limit;
    const rmod_xml_config_entry config_children[] =  {
            {.name = "sim_time", .child_count = 0, .converter = {.c_real = {.p_out = &sim_time, .v_max = INFINITY, .v_min = FLT_EPSILON, .type = RMOD_CFG_VALUE_REAL }}},
            {.name = "sim_reps", .child_count = 0, .converter = {.c_uint = {.p_out = &sim_reps, .v_min = 1, .v_max = UINTMAX_MAX, .type = RMOD_CFG_VALUE_UINT}}},
            {.name = "filename", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_filename}}},
            {.name = "chain", .child_count = 0, .converter = {.c_str = {.type = RMOD_CFG_VALUE_STR, .p_out = &segment_chain}}},
            {.name = "threads", .child_count = 0, .converter = {.c_uint = {.type = RMOD_CFG_VALUE_UINT, .v_min = 0, .v_max = 256, .p_out = &thrd_count}}},
            {.name = "repair_limit", .child_count = 0, .converter = {.c_real = {.type = RMOD_CFG_VALUE_REAL, .v_min = 0.0, .v_max = INFINITY, .p_out = &repair_limit}}}
    };
    const rmod_xml_config_entry config_master =
            {
            .name = "config",
            .child_count = sizeof(config_children) / sizeof(*config_children),
            .child_array = config_children,
            };

    rmod_memory_file cfg_file;
    printf("Parsing job description from file \"%s\"\n", arg_job_desc);
    res = rmod_parse_configuration_file(arg_job_desc, &config_master, &cfg_file);
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
    if (!realpath(arg_job_desc, program_filename))
    {
        RMOD_ERROR_CRIT("Could not find full path to file \"%s\", reason: %s", arg_job_desc, RMOD_ERRNO_MESSAGE);
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



    rmod_sim_result results = {0};
    if (thrd_count < 2)
    {
        printf("Simulating graph built from chain \"%s\" containing %"PRIuFAST32" individual nodes\n", graph_a.graph_type, graph_a.node_count);
        res = rmod_simulate_graph(&graph_a, (f32) sim_time, (u32) sim_reps, &results, (f32) repair_limit);
        if (res != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR_CRIT("Failed simulating graph [%s - %s], reason: %s", graph_a.module_name, graph_a.graph_type, rmod_result_str(res));
        }
    }
    else
    {
        printf("Simulating graph built from chain \"%s\" containing %"PRIuFAST32" individual nodes using %u threads\n", graph_a.graph_type, graph_a.node_count, (u32)thrd_count);
        res = rmod_simulate_graph_mt(&graph_a, (f32) sim_time, (u32) sim_reps, &results, thrd_count, (f32)repair_limit);
        if (res != RMOD_RESULT_SUCCESS)
        {
            RMOD_ERROR_CRIT("Failed simulating graph [%s - %s], reason: %s", graph_a.module_name, graph_a.graph_type, rmod_result_str(res));
        }
    }

#ifndef NDEBUG
    printf("Rng was called %"PRIu64" times\n", MSWS_TIMES_CALLED);
#endif //NDEBUG

//    const f64 avg_flow = (f64) (results.total_flow) / (f64) results.sim_count / sim_time;
//    printf("Simulation results (took %g s of processor time for %lu runs, or %g s per run):\n\tMax flow: %g\n\tAvg flow: %g (%3.2f%% availability)\n\tAvg maintenance visits: %g\n\tAvg costs: %g\n", results.duration, results.sim_count, (f64)results.duration / (f64)results.sim_count, (f64)results.max_flow,
//           avg_flow, avg_flow / (f64)results.max_flow * 100.0, (f64)(results.total_maintenance_visits) / (f64)results.sim_count, (f64)(results.total_costs) / (f64)results.sim_count);
//
//    printf("Mean failures per component:\n");
//    u32* const fails_per_type = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*fails_per_type) * graph_a.type_count);
//    if (!fails_per_type)
//    {
//        RMOD_ERROR_CRIT("Failed allocating memory for array of fails per component type: failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*fails_per_type) * graph_a.type_count);
//    }
//    memset(fails_per_type, 0, sizeof(*fails_per_type) * graph_a.type_count);
//    for (u32 i = 0; i < results.n_components; ++i)
//    {
//        const f64 avg_fails = (f64) results.failures_per_component[i] / (f64) sim_reps;
//        printf("\t%u - %.*s: %lu (avg of %g, MTBF: %g)\n", i, graph_a.parent->chain_elements[i].label.len, graph_a.parent->chain_elements[i].label.begin, results.failures_per_component[i], avg_fails, (f64)sim_time / avg_fails);
//        fails_per_type[graph_a.node_list[i].type_id] += results.failures_per_component[i];
//    }
//
//    printf("Failures per type:\n");
//    u64 total_failures = 0;
//    for (u32 i = 0; i < graph_a.type_count; ++i)
//    {
//        printf("\t%s: %u\n", graph_a.type_list[i].name, fails_per_type[i]);
//        total_failures += fails_per_type[i];
//    }
//    lin_jfree(G_LIN_JALLOCATOR, fails_per_type);
//    printf("Total failures: %"PRIu64"\n", total_failures);

    string_stream* ss_out;
    if (string_stream_create(G_LIN_JALLOCATOR, G_JALLOCATOR, &ss_out))
    {
        RMOD_ERROR_CRIT("Failed creating output string stream, reason: %s", rmod_result_str(RMOD_RESULT_NOMEM));
    }

    res = rmod_postprocess_results(&results, sim_time, repair_limit, &graph_a, thrd_count, &program, argc, argv, ss_out);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR_CRIT("Could not postprocess simulation results, reason: %s", rmod_result_str(res));
    }

    if (out_file_name_segment.len && out_file_name_segment.begin)
    {
        printf("Saving simulation output to file \"%s\"\n", out_file_name_segment.begin);
        setlocale(LC_ALL, "en_US.UTF-8");
        FILE* f_out = fopen(out_file_name_segment.begin, "w");
        if (!f_out)
        {
            RMOD_ERROR_CRIT("Failed opening file \"%s\" to output results, reason: %s", out_file_name_segment.begin, RMOD_ERRNO_MESSAGE);
        }
        const u8* str = (u8*)string_stream_contents(ss_out);
        while (*str)
        {
            assert(*str <= 127);
            str += 1;
        }
        fprintf(f_out, "%s\n", string_stream_contents(ss_out));
        if (ferror(f_out))
        {
            fclose(f_out);
            RMOD_ERROR_CRIT("Error occurred while trying to write to file \"%s\", reason: %s", out_file_name_segment.begin, RMOD_ERRNO_MESSAGE);
        }
        fclose(f_out);
    }
    else
    {
        printf("%s", string_stream_contents(ss_out));
    }

    string_stream_cleanup(ss_out);
    ss_out = NULL;

    printf("Cleaning up\n");
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
        uint_fast64_t max_alloc_size, total_allocated, max_usage, alloc_count;
        jallocator_destroy(jallocator);
    }
    {
        linear_jallocator* const lin_jallocator = G_LIN_JALLOCATOR;
        G_LIN_JALLOCATOR = NULL;
        lin_jallocator_destroy(lin_jallocator);
    }
    RMOD_LEAVE_FUNCTION;
    rmod_error_cleanup_thread();
    printf("Run complete\n");
    return 0;
}
