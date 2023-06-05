//
// Created by jan on 2.6.2023.
//

#include "simulation_run.h"

static inline f32 find_next_failure(f64 (* rng_function)(void* param), void* rng_param, f32 failure_rate)
{
//    return (f32)(- log(1.0 - rng_function(rng_param)) / (f64)failure_rate);
    return -(f32)log(1.0 - rng_function(rng_param)) / (f32)failure_rate;
}

static f32 find_system_throughput(
        f32*const sys_failure, const u32 count, const rmod_element_status* const status, const u32* const parent_count,
        const rmod_graph_node_id* const* const parent_array, const f32* const effect, f32* value,
        const f32* const failure_rate)
{
    f32 total_failure_rate = 0;
    for (u32 i = 0; i < count; ++i)
    {
        if (status[i] == RMOD_ELEMENT_STATUS_WORK)
        {
            f32 input = (f32)(parent_count[i] == 0);
            const rmod_graph_node_id* parents = parent_array[i];
            for (u32 j = 0; j < parent_count[i]; ++j)
            {
                input += value[parents[j]];
            }
            value[i] = effect[i] * input;
            total_failure_rate += failure_rate[i];
        }
        else
        {
            value[i] = 0;
        }
    }
    *sys_failure = total_failure_rate;
    return value[count - 1];
}

rmod_result rmod_simulate_graph(
        const rmod_graph* graph, f32 sim_time, u32 sim_times, rmod_sim_result* p_res_out,
        f64 (* rng_function)(void* param), void* rng_param)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* const base = lin_jalloc_get_current(G_LIN_JALLOCATOR);

    //  Reliability of each element
    const u32 count = graph->node_count;
    f32* const failure_rate = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_rate) * count);
    if (!failure_rate)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_rate) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Cost of each element
    f32* const cost = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*cost) * count);
    if (!cost)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*cost) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Effect of each element
    f32* const effect = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*effect) * count);
    if (!effect)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*effect) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Failure type of each element
    rmod_failure_type* const failure = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure) * count);
    if (!failure)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent counts of each element
    u32* const parent_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_count) * count);
    if (!parent_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_count) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent arrays of each element
    const rmod_graph_node_id** const parent_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_array) * count);
    if (!parent_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_array) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Value of each element (for computing flow through the graph)
    f32* const value = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*value) * count);
    if (!value)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*value) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Status of each element (for computing flow through the graph)
    rmod_element_status* const status = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*status) * count);
    if (!status)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*status) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    u32* const fails_per_component = jalloc(sizeof(*fails_per_component) * count);
    if (!fails_per_component)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof(*fails_per_component) * count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }


    //  Prepare invariant values (failure rates, cost, effect, failure, parent counts, and parent arrays)
    for (u32 i = 0; i < count; ++i)
    {
        const rmod_graph_node* node = graph->node_list + i;
        const rmod_graph_node_type* type = graph->type_list + node->type_id;
        failure_rate[i] = type->failure_rate;
        cost[i] = type->cost;
        effect[i] = type->effect;
        failure[i] = type->failure_type;
        parent_count[i] = node->parent_count;
        parent_array[i] = node->parents;
        fails_per_component[i] = 0;
    }

    //  Simulation records
    rmod_sim_result results =
            {
            .total_failures = 0,
            .total_flow = 0,
            .sim_count = 0,
            .n_components = 0,
            .total_costs = 0,
            .duration = 0,
            .failures_per_component = NULL,
            };

#ifndef _WIN32
    struct timespec t_begin;
    int time_res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_begin);
    assert(time_res >= 0);
#else
#error NOT IMPLEMENTED
#endif
#define N_MILESTONES 40
    u32 milestones[N_MILESTONES] = {};
    for (u32 i = 0; i < N_MILESTONES; ++i)
    {
        milestones[i] = (u32)((f64)sim_times / (f64)N_MILESTONES * (f64)i);
    }
    u32 milestone = 0;
    fprintf(stdout, "Simulation progress (  0.0 %%): [----------------------------------------]");
    fflush(stdout);
    for (u32 sim_i = 0; sim_i < sim_times; ++sim_i)
    {
        if (sim_i == milestones[milestone])
        {
            fprintf(stdout, "\rSimulation progress (%5.1f %%): [", (f64)sim_i / (f64)sim_times * 100);
            u32 i;
            for (i = 0; i < milestone; ++i)
            {
                fputc('=', stdout);
            }
            while (i < N_MILESTONES)
            {
                fputc('-', stdout);
                i += 1;
            }
            fputc(']', stdout);
            fflush(stdout);
            milestone += 1;
        }
        f32 system_failure_rate = 0.0f;
        f32 total_throughput = 0.0f;
        f32 time = 0.0f;
        f32 total_cost = 0.0f;
        //  Reset status before simulation
        for (u32 i = 0; i < count; ++i)
        {
            status[i] = RMOD_ELEMENT_STATUS_WORK;
        }
        value[0] = effect[0];
        //  Compute graph throughput
        system_failure_rate = 0.0f;

        u32 maintenance_count = 0;
        f32 throughput = find_system_throughput(&system_failure_rate, count, status, parent_count, parent_array, effect, value, failure_rate);
        while (time < sim_time)
        {
            //  Find time step
            f32 dt = find_next_failure(rng_function, rng_param, system_failure_rate);
            f32 next_t = time + dt;
            if (next_t > sim_time)
            {
                dt = sim_time - time;
                time = sim_time;
                total_throughput += throughput * dt;
                //  Simulation is done
                break;
            }

            total_throughput += throughput * dt;

            //  Find which failure occurs next
            u32 fail_idx = -1;
            f32 failed_so_far = 0.0f;
            const f32 fail_measure = (f32) rng_function(rng_param) * system_failure_rate;
            for (u32 i = 0; i < count; ++i)
            {
                if (status[i] != RMOD_ELEMENT_STATUS_WORK)
                {
                    continue;
                }
                failed_so_far += failure_rate[i];
                if ((failed_so_far) >= fail_measure)
                {
                    fail_idx = i;
                    break;
                }
            }
            assert(fail_idx < count);
            assert(fail_idx != -1);

            const rmod_failure_type type = failure[fail_idx];
            status[fail_idx] = RMOD_ELEMENT_STATUS_DOWN;
            fails_per_component[fail_idx] += 1;
            if (type == RMOD_FAILURE_TYPE_FATAL)
            {
                break;
            }

            if (type == RMOD_FAILURE_TYPE_ACCEPTABLE)
            {
                //  Failure (may) be acceptable
                throughput = find_system_throughput(&system_failure_rate, count, status, parent_count, parent_array, effect, value, failure_rate);
            }

            if (throughput == 0.0f || type == RMOD_FAILURE_TYPE_CRITICAL)
            {
                //  Need to maintain, so repair all systems
                for (u32 i = 0; i < count; ++i)
                {
                    if (status[i] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        total_cost += cost[i];
                        status[i] = RMOD_ELEMENT_STATUS_WORK;
                    }
                }
                throughput = find_system_throughput(&system_failure_rate, count, status, parent_count, parent_array, effect, value, failure_rate);
                maintenance_count += 1;
            }

            time = next_t;
        }

        results.total_flow += total_throughput;
        results.total_failures += maintenance_count;
        results.sim_count += 1;
        results.total_costs += total_cost;
    }
    results.n_components = count;
    results.failures_per_component = fails_per_component;
    fprintf(stdout, "\rSimulation progress (100.0 %%): [========================================]\n");
    fflush(stdout);

#ifndef _WIN32
    struct timespec t_end;
    time_res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_end);
    assert(time_res >= 0);
    results.duration = (f32)(t_end.tv_sec - t_begin.tv_sec);
    {
        f64 ns = (f64)(t_end.tv_nsec - t_begin.tv_nsec);
        results.duration += (f32)(ns / 1e9);
    }
#else
#error NOT IMPLEMENTED
#endif
    fprintf(stdout, "\nSim concluded in %g seconds\n", results.duration);

    lin_jfree(G_LIN_JALLOCATOR, status);
    lin_jfree(G_LIN_JALLOCATOR, value);
    lin_jfree(G_LIN_JALLOCATOR, parent_array);
    lin_jfree(G_LIN_JALLOCATOR, parent_count);
    lin_jfree(G_LIN_JALLOCATOR, failure);
    lin_jfree(G_LIN_JALLOCATOR, effect);
    lin_jfree(G_LIN_JALLOCATOR, cost);
    lin_jfree(G_LIN_JALLOCATOR, failure_rate);

    *p_res_out = results;

end:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}
