//
// Created by jan on 2.6.2023.
//

#include "simulation_run.h"

static inline f32 find_next_failure(rmod_acorn_state* acorn, f32 failure_rate)
{
    return (f32)(- log(1.0 - acorn_rngf(acorn)) / (f64)failure_rate);
}

rmod_result rmod_simulate_graph(rmod_acorn_state* acorn, const rmod_graph* graph, f32 sim_time, u32 sim_times, rmod_sim_result* p_res_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* const base = lin_jalloc_get_current(G_LIN_JALLOCATOR);

    //  Reliability of each element
    f32* failure_rates = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_rates) * graph->node_count);
    if (!failure_rates)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_rates) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Cost of each element
    f32* cost = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*cost) * graph->node_count);
    if (!cost)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*cost) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Effect of each element
    f32* effect = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*effect) * graph->node_count);
    if (!effect)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*effect) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Failure type of each element
    rmod_failure_type* failure = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure) * graph->node_count);
    if (!failure)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent counts of each element
    u32* parent_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_count) * graph->node_count);
    if (!parent_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_count) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent arrays of each element
    const rmod_graph_node_id** parent_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_array) * graph->node_count);
    if (!parent_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_array) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Value of each element (for computing flow through the graph)
    f32* value = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*value) * graph->node_count);
    if (!value)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*value) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Status of each element (for computing flow through the graph)
    rmod_element_status* status = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*status) * graph->node_count);
    if (!status)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*status) * graph->node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }


    //  Prepare invariant values (failure rates, cost, effect, failure, parent counts, and parent arrays)
    for (u32 i = 0; i < graph->node_count; ++i)
    {
        const rmod_graph_node* node = graph->node_list + i;
        const rmod_graph_node_type* type = graph->type_list + node->type_id;
        failure_rates[i] = type->failure_rate;
        cost[i] = type->cost;
        effect[i] = type->effect;
        failure[i] = type->failure_type;
        parent_count[i] = node->parent_count;
        parent_array[i] = node->parents;
    }

    //  Simulation records
    rmod_sim_result results =
            {
            .avg_fail_count = 0,
            .avg_flow = 0,
            .sim_count = 0,
            };
    f32 system_failure_rate = 0.0f;




end:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}
