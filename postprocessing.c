//
// Created by jan on 10.6.2023.
//

#include "postprocessing.h"
#include <time.h>
#include <inttypes.h>

#define sstream_print(stream, fmt, ...) if (string_stream_add(sstream, fmt __VA_OPT__(,) __VA_ARGS__) == -1) {RMOD_ERROR("Failed message to string stream"); res = RMOD_RESULT_NOMEM; goto failed;} (void)0

rmod_result rmod_postprocess_results(
        const rmod_sim_result* results, f64 sim_duration, f64 repair_limit, const rmod_graph* graph, u32 thread_count,
        const rmod_program* program, int argc, const char* argv[], string_stream* sstream)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res;
    void* base = lin_jalloc_get_current(G_LIN_JALLOCATOR);
    char* intermediate_buffer = lin_jalloc(G_LIN_JALLOCATOR, 4096);
    if (!intermediate_buffer)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, 4096);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    struct tm now;
    const time_t t_now = time(NULL);
    localtime_r(&t_now, &now);
    strftime(intermediate_buffer, 4096, "%x at %T", &now);
    sstream_print(sstream, "Results of simulation on %s:\n\tCalled with arguments:\n", intermediate_buffer);
    lin_jfree(G_LIN_JALLOCATOR, intermediate_buffer);
    for (u32 i = 0; i < argc; ++i)
    {
        sstream_print(sstream, "\t\t[%u] - \"%s\"\n", i, argv[i]);
    }
    const rmod_chain* const chain = graph->parent;
    uint_fast64_t max_alloc_size;
    uint_fast64_t total_allocated;
    uint_fast64_t max_usage;
    uint_fast64_t alloc_count;
    jallocator_statistics(G_JALLOCATOR, &max_alloc_size, &total_allocated, &max_usage, &alloc_count);
    total_allocated += lin_jallocator_get_size(G_LIN_JALLOCATOR);
    max_usage += lin_jallocator_get_size(G_LIN_JALLOCATOR);
    f64 mean_flow = results->total_flow / (sim_duration * (f64)results->sim_count);
    sstream_print(sstream, "\n\tSimulation overview:\n"
                           "\t\tMean flow: %g (%.2f%% availability)\n"
                           "\t\tMean maintenance visits: %"PRIu64"\n"
                           "\t\tProcessor time used: %f seconds\n"
                           "\t\tMaximum allocation size: %g kB (%g MB)\n"
                           "\t\tTotal allocated size: %g kb (%g MB)\n"
                           "\t\tMaximum usage: %g kb (%g MB)\n"
                           "\t\tTotal allocation count: %"PRIuFAST64"\n",
                  mean_flow,
                  mean_flow / results->max_flow * 100.0,
                  results->total_maintenance_visits,
                  results->duration,
                           ((f64)max_alloc_size) / (1 << 10), ((f64)max_alloc_size) / (1 << 20),
                           ((f64)total_allocated) / (1 << 10), ((f64)total_allocated) / (1 << 20),
                           ((f64)max_usage) / (1 << 10), ((f64)max_usage) / (1 << 20),
                  max_usage);
    sstream_print(sstream, "\n\tInput parameter overview:\n"
                           "\t\tSimulation duration: %g\n"
                           "\t\tSimulation repetitions: %"PRIu64"\n"
                           "\t\tRepair limit: %g\n"
                           "\t\tChain used: \"%.*s\"\n"
                           "\t\tThreads used: %u\n",
                           sim_duration,
                           results->sim_count,
                           repair_limit,
                           chain->header.type_name.len, chain->header.type_name.begin,
                           thread_count);
    
    sstream_print(sstream, "\n\tComponent results:");
    u32* const fails_per_type = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*fails_per_type) * program->n_types);
    if (!fails_per_type)
    {
        RMOD_ERROR("Failed allocating memory for array of fails per component type: failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*fails_per_type) * program->n_types);
        goto failed;
    }
    u32* const component_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*component_count) * program->n_types);
    if (!component_count)
    {
        RMOD_ERROR("Failed allocating memory for array of fails per component type: failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*component_count) * program->n_types);
        goto failed;
    }
    memset(fails_per_type, 0, sizeof(*fails_per_type) * program->n_types);
    memset(component_count, 0, sizeof(*component_count) * program->n_types);
    for (u32 i = 0; i < chain->element_count; ++i)
    {
        const rmod_chain_element* const element = chain->chain_elements + i;
        const rmod_element_type* const type = program->p_types + element->type_id;
        assert(type->header.type_value == RMOD_ELEMENT_TYPE_BLOCK);
        const rmod_block* const block = &type->block;
        const f64 mtbf = (f64)results->sim_count * sim_duration/ (f64)results->failures_per_component[i];
        fails_per_type[element->type_id] += results->failures_per_component[i];
        component_count[element->type_id] += 1;
        sstream_print(sstream,"\n\t\tComponent \"%.*s\"\n"
                              "\t\t\tType's name: \"%.*s\"\n"
                              "\t\t\tType's MTBF: %g\n"
                              "\t\t\tActual MTBF: %g\n"
                              "\t\t\tTimes failed: %"PRIu64"\n"
                              "\t\t\tTotal costs: %f\n"
                              "\t\t\tAverage costs: %f\n"
                              "\t\t\tParents:\n",
                      element->label.len, element->label.begin,
                      block->header.type_name.len, block->header.type_name.begin,
                      block->mtbf,
                      mtbf,
                      results->failures_per_component[i],
                      (f64)results->failures_per_component[i] * block->cost,
                      (f64)results->failures_per_component[i] * block->cost / (f64)results->sim_count
                      );
        for (u32 j = 0; j < element->parent_count; ++j)
        {
            const rmod_chain_element* parent = element + element->parents[j];
            sstream_print(sstream, "\t\t\t\t\"%.*s\"\n", parent->label.len, parent->label.begin);
        }
        sstream_print(sstream, "\t\t\tChildren:\n");
        for (u32 j = 0; j < element->child_count; ++j)
        {
            const rmod_chain_element* child = element + element->children[j];
            sstream_print(sstream, "\t\t\t\t\"%.*s\"\n", child->label.len, child->label.begin);
        }
    }

    
    sstream_print(sstream, "\n\tType results:");
    for (u32 i = 0; i < graph->type_count; ++i)
    {
        const rmod_block* block = NULL;
        const rmod_graph_node_type* type = graph->type_list + i;
        const u64 type_name_len = strlen((const char*)type->name);
        for (u32 j = 0; j < program->n_types; ++j)
        {
            const rmod_element_type* current = program->p_types + j;
            if (current->header.type_value == RMOD_ELEMENT_TYPE_BLOCK && current->header.type_name.len == type_name_len &&
                    strncmp((const char*)type->name, current->header.type_name.begin, current->header.type_name.len) == 0)
            {
                block = &(program->p_types + j)->block;
                break;
            }
        }
        if (!block)
        {
            RMOD_ERROR("No block type with name \"%s\" was found", type->name);
            res = RMOD_RESULT_STUPIDITY;
            goto failed;
        }

        f64 mtbf = (f64)results->sim_count * sim_duration/ (f64)fails_per_type[i] / (f64)component_count[i];
        sstream_print(sstream,"\n\t\tType \"%s\"\n"
                              "\t\t\tTotal components: %u\n"
                              "\t\t\tTotal failures: %"PRIu32"\n"
                              "\t\t\tType's MTBF: %g\n"
                              "\t\t\tActual MTBF: %g\n"
                              "\t\t\tTotal costs: %f\n"
                              "\t\t\tAverage costs: %f\n"
                              "\t\t\tFailure type: %s\n"
                              "\t\t\tEffect: %g\n",
                              type->name,
                              component_count[i],
                              fails_per_type[i],
                              block->mtbf,
                              mtbf,
                              (f64)fails_per_type[i] * type->cost,
                              (f64)fails_per_type[i] * type->cost / sim_duration / (f64)component_count[i],
                              rmod_failure_type_to_str(type->failure_type),
                              type->effect);
    }

    lin_jfree(G_LIN_JALLOCATOR, component_count);
    lin_jfree(G_LIN_JALLOCATOR, fails_per_type);
    

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

failed:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}
