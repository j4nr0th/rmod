//
// Created by jan on 2.6.2023.
//

#include "simulation_run.h"
#include "random/msws.h"
#include <pthread.h>
#include <stdio.h>

static inline f32 find_next_failure(rmod_msws_state* rng, f32 failure_rate)
{
    return -(f32)log(1.0 - rmod_msws_rngf(rng)) / (f32)failure_rate;
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


static void report_sim_progress(f64 fraction, u32 signs)
{
    const u32 limit = (u64)(fraction * signs);
    fprintf(stdout, "\rSimulation progress (%5.1f %%): [\x1b[32m", 100.0 * fraction);
    u32 i;
    for (i = 0; i < limit; ++i)
    {
        fputc('=', stdout);
    }
    fputc('\x1b', stdout);
    fputc('[', stdout);
    fputc('0', stdout);
    fputc('m', stdout);
    for (; i < signs; ++i)
    {
        fputc('-', stdout);
    }
    fputc(']', stdout);
    fflush(stdout);
}

rmod_result rmod_simulate_graph(
        const rmod_graph* graph, f32 simulation_duration, u32 simulation_repetitions, rmod_sim_result* p_res_out,
        f32 repair_limit)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* const base = lin_jalloc_get_current(G_LIN_JALLOCATOR);

    //  Reliability of each element
    const u32 node_count = graph->node_count;
    f32* const failure_rate = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_rate) * node_count);
    if (!failure_rate)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_rate) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Cost of each element
    f32* const cost = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*cost) * node_count);
    if (!cost)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*cost) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Effect of each element
    f32* const effect = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*effect) * node_count);
    if (!effect)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*effect) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Failure type of each element
    rmod_failure_type* const failure_type = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_type) * node_count);
    if (!failure_type)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_type) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent counts of each element
    u32* const parent_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_count) * node_count);
    if (!parent_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_count) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Parent arrays of each element
    const rmod_graph_node_id** const parent_ids = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_ids) * node_count);
    if (!parent_ids)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_ids) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Child counts of each element
    u32* const child_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*child_count) * node_count);
    if (!child_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*child_count) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Child arrays of each element
    const rmod_graph_node_id** const child_ids = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*child_ids) * node_count);
    if (!child_ids)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*child_ids) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Value of each element (for computing flow through the graph)
    f32* const value = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*value) * node_count);
    if (!value)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*value) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    //  Status of each element (for computing flow through the graph)
    rmod_element_status* const node_status = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*node_status) * node_count);
    if (!node_status)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*node_status) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    u64* const fails_per_component = jalloc(sizeof(*fails_per_component) * node_count);
    if (!fails_per_component)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof(*fails_per_component) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }

    u32 total_children = 0, total_parents = 0;
    //  Prepare invariant values (failure rates, cost, effect, failure, parent counts, and parent arrays)
    for (u32 i = 0; i < node_count; ++i)
    {
        const rmod_graph_node* node = graph->node_list + i;
        const rmod_graph_node_type* type = graph->type_list + node->type_id;
        failure_rate[i] = type->failure_rate;
        cost[i] = type->cost;
        effect[i] = type->effect;
        failure_type[i] = type->failure_type;
        total_parents += (parent_count[i] = node->parent_count);
        total_children += (child_count[i] = node->child_count);
        parent_ids[i] = node->parents;
        child_ids[i] = node->children;
        fails_per_component[i] = 0;
    }


    //  Simulation records
    rmod_sim_result results =
            {
            .total_maintenance_visits = 0,
            .total_flow = 0,
            .sim_count = 0,
            .n_components = 0,
            .total_costs = 0,
            .duration = 0,
            .failures_per_component = NULL,
            };

    f32 full_fail_rate = 0.0f;
    value[0] = effect[0];
    f32 full_throughput;
    //  Reset status before simulation
    for (u32 i = 0; i < node_count; ++i)
    {
        node_status[i] = RMOD_ELEMENT_STATUS_WORK;
    }
    if ((full_throughput = find_system_throughput(&full_fail_rate, node_count, node_status, parent_count, parent_ids, effect, value, failure_rate)) < repair_limit)
    {
        RMOD_ERROR("Maximum graph throughput was %g, however minimum value for repair was specified to be %g. This is most likely due to incorrect specification", full_throughput, repair_limit);
        jfree(fails_per_component);
        res = RMOD_RESULT_BAD_VALUE;
        goto end;
    }


    rmod_msws_state rng;
    rmod_msws_init(&rng, 0, 0, 0, 0);
#ifndef _WIN32
    struct timespec t_begin;
    int time_res = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t_begin);
    assert(time_res >= 0);
#else
    LARGE_INTEGER t_begin;
    QueryPerformanceCounter(&t_begin);
#endif
#define N_MILESTONES 100
    u32 milestones[N_MILESTONES] = {0};
    for (u32 i = 0; i < N_MILESTONES; ++i)
    {
        milestones[i] = (u32)((f64)simulation_repetitions / (f64)N_MILESTONES * (f64)i);
    }
    u32 milestone = 0;
    fprintf(stdout, "Simulation progress (  0.0 %%): [----------------------------------------]");
    fflush(stdout);
    for (u32 sim_i = 0; sim_i < simulation_repetitions; ++sim_i)
    {
        if (sim_i == milestones[milestone])
        {
//            fprintf(stdout, "\rSimulation progress (%5.1f %%): [", (f64)sim_i / (f64)simulation_repetitions * 100);
//            u32 i;
//            for (i = 0; i < milestone; ++i)
//            {
//                fputc('=', stdout);
//            }
//            while (i < N_MILESTONES)
//            {
//                fputc('-', stdout);
//                i += 1;
//            }
//            fputc(']', stdout);
//            fflush(stdout);
            report_sim_progress((f64)milestone / N_MILESTONES, 40);
            milestone += 1;
        }
        f32 system_failure_rate = 0.0f;
        f32 total_throughput = 0.0f;
        f32 time = 0.0f;
        f32 total_cost = 0.0f;
        //  Reset status before simulation
        for (u32 i = 0; i < node_count; ++i)
        {
            node_status[i] = RMOD_ELEMENT_STATUS_WORK;
        }
        //  Compute graph throughput
        system_failure_rate = 0.0f;

        u32 maintenance_count = 0;
        f32 throughput = find_system_throughput(&system_failure_rate, node_count, node_status, parent_count, parent_ids, effect, value, failure_rate);
        while (time < simulation_duration)
        {
            //  Find time step
            f32 dt = find_next_failure(&rng, system_failure_rate);
            f32 next_t = time + dt;
            if (next_t > simulation_duration)
            {
                dt = simulation_duration - time;
                time = simulation_duration;
                total_throughput += throughput * dt;
                //  Simulation is done
                break;
            }

            total_throughput += throughput * dt;

            //  Find which failure occurs next
            u32 fail_idx = -1;
            f32 failed_so_far = 0.0f;
            const f32 fail_measure = (f32) find_next_failure(&rng, system_failure_rate) * system_failure_rate;
            for (u32 i = 0; i < node_count; ++i)
            {
                if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
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
            assert(fail_idx < node_count);
            assert(fail_idx != -1);
            const rmod_failure_type type = failure_type[fail_idx];
            node_status[fail_idx] = RMOD_ELEMENT_STATUS_DOWN;
            fails_per_component[fail_idx] += 1;
            if (type == RMOD_FAILURE_TYPE_FATAL)
            {
                break;
            }

            if (type == RMOD_FAILURE_TYPE_ACCEPTABLE)
            {
                //  Deactivate all components which provide/depend only on this one, since there is no point in having
                //  them running while the only part they depend on/provide for is not online
                //  Failure (may) be acceptable

                // Check for all nodes after the failed node if they should be switched off
                u32 working_parents = 0;
                for (u32 i = fail_idx + 1; i < node_count; ++i)
                {
                    //  Is the node even working
                    if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        continue;
                    }
                    //  Count how many of its parents are still working
                    for (u32 j = 0; j < parent_count[i]; ++j)
                    {
                        working_parents += node_status[parent_ids[i][j]] == RMOD_ELEMENT_STATUS_WORK;
                    }
                    if (working_parents == 0)
                    {
                        //  No more working parents, switch it off
                        node_status[i] = RMOD_ELEMENT_STATUS_INACTIVE;
                    }
                }
                if (!working_parents)
                {
                    //  Last node in the graph has no more working parents
                    goto repair;
                }

                u32 working_children = 0;
                //  Check for all nodes before the failed node if they should be switched off
                for (u32 i = fail_idx; i > 0; --i)
                {
                    //  Is the node even working
                    if (node_status[i - 1] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        continue;
                    }
                    //  Count how many of its children are still working
                    for (u32 j = 0; j < child_count[i - 1]; ++j)
                    {
                        working_children += node_status[child_ids[i - 1][j]] == RMOD_ELEMENT_STATUS_WORK;
                    }
                    if (working_children == 0)
                    {
                        //  No more working parents, switch it off
                        node_status[i - 1] = RMOD_ELEMENT_STATUS_INACTIVE;
                    }
                }
                if (!working_children)
                {
                    //  First node in the graph has no more working children
                    //  Literally should not happen
                    assert(false);
                }
                throughput = find_system_throughput(&system_failure_rate, node_count, node_status, parent_count, parent_ids, effect, value, failure_rate);
            }

            if (throughput < repair_limit || type == RMOD_FAILURE_TYPE_CRITICAL)
            {
            repair:
                //  Need to maintain, so repair all systems
                for (u32 i = 0; i < node_count; ++i)
                {
                    if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        //  Add repair cost if element was down, but not if it was just shut down
                        total_cost += node_status[i] == RMOD_ELEMENT_STATUS_DOWN ? cost[i] : 0.0f;
                        node_status[i] = RMOD_ELEMENT_STATUS_WORK;
                    }
                }
                throughput = full_throughput;
                system_failure_rate = full_fail_rate;
                maintenance_count += 1;
            }

            time = next_t;
        }

        results.total_flow += total_throughput;
        results.total_maintenance_visits += maintenance_count;
        results.sim_count += 1;
        results.total_costs += total_cost;
    }
    results.n_components = node_count;
    results.failures_per_component = fails_per_component;
    report_sim_progress(1.0, 40);
#ifndef _WIN32
    struct timespec t_end;
    time_res = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t_end);
    assert(time_res >= 0);
    results.duration = (f32)(t_end.tv_sec - t_begin.tv_sec);
    results.max_flow = full_throughput;
    {
        f64 ns = (f64)(t_end.tv_nsec - t_begin.tv_nsec);
        results.duration += (f32)(ns / 1e9);
    }
#else
    LARGE_INTEGER t_end, freq;
    QueryPerformanceCounter(&t_end);
    QueryPerformanceFrequency(&freq);
    results.duration = (f32)((f64)(t_end.QuadPart - t_begin.QuadPart) / (f64)freq.QuadPart);
#endif
    fprintf(stdout, "\nSimulation took in %g seconds of CPU time\n", results.duration);


    lin_jfree(G_LIN_JALLOCATOR, node_status);
    lin_jfree(G_LIN_JALLOCATOR, value);
    lin_jfree(G_LIN_JALLOCATOR, child_ids);
    lin_jfree(G_LIN_JALLOCATOR, child_count);
    lin_jfree(G_LIN_JALLOCATOR, parent_ids);
    lin_jfree(G_LIN_JALLOCATOR, parent_count);
    lin_jfree(G_LIN_JALLOCATOR, failure_type);
    lin_jfree(G_LIN_JALLOCATOR, effect);
    lin_jfree(G_LIN_JALLOCATOR, cost);
    lin_jfree(G_LIN_JALLOCATOR, failure_rate);

    *p_res_out = results;

end:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}

typedef struct
{
    u32 node_count;
    u32 reps_to_do;
    f32 duration;
    f32 repair_limit;
    f32 full_fail_rate;
    f32 full_throughput;
    const f32* failure_rate;
    const f32* cost;
    const f32* effect;
    const rmod_failure_type* failure_type;
    const u32* parent_count;
    const rmod_graph_node_id*const* parent_ids;
    const u32* child_count;
    const rmod_graph_node_id* const* child_ids;
} simulation_parameters;

typedef struct
{
    u32 worker_thread_id;
    rmod_msws_state* p_rng_state;
    rmod_sim_result* p_sim_results;
    u32* p_reps_done;
    //  These are invariant (shared between threads)
    const simulation_parameters* sim_params;
    //  These are variable (thread specific)
    f32* value;
    rmod_element_status* node_status;
    u64* fails_per_component;
} graph_worker_information;

static rmod_result simulate_worker(const simulation_parameters* params, rmod_msws_state* const rng, u32* const pi_sim, f32* const value, rmod_element_status* const node_status, u64*const fails_per_component, rmod_sim_result* const p_res_out)
{
    RMOD_ENTER_FUNCTION;
    const u32 reps_to_do = params->reps_to_do;
    const f32 duration = params->duration;
    const u32 node_count = params->node_count;
    const f32* failure_rate = params->failure_rate;
    const f32* cost = params->cost;
    const f32* effect = params->effect;
    const rmod_failure_type* failure_type = params->failure_type;
    const u32* parent_count = params->parent_count;
    const rmod_graph_node_id*const* parent_ids = params->parent_ids;
    const u32* child_count = params->child_count;
    const rmod_graph_node_id*const* child_ids = params->child_ids;
    const f32 repair_limit = params->repair_limit;
    const f32 full_throughput = params->full_throughput;
    const f32 full_fail_rate = params->full_fail_rate;


    rmod_sim_result results =
            {
            .duration = 0,
            .total_costs = 0,
            .sim_count = 0,
            .total_maintenance_visits = 0,
            .total_flow = 0,
            .failures_per_component = 0,
            .n_components = 0,
            };
    u32 sim_i;
    for (sim_i = 0; sim_i < reps_to_do; ++sim_i)
    {
        *pi_sim = sim_i;
        f32 system_failure_rate = 0.0f;
        f32 total_throughput = 0.0f;
        f32 time = 0.0f;
        f32 total_cost = 0.0f;
        //  Reset status before simulation
        for (u32 i = 0; i < node_count; ++i)
        {
            node_status[i] = RMOD_ELEMENT_STATUS_WORK;
        }
        value[0] = effect[0];
        //  Compute graph throughput
        system_failure_rate = 0.0f;

        u32 maintenance_count = 0;
        f32 throughput = find_system_throughput(&system_failure_rate, node_count, node_status, parent_count, parent_ids, effect, value, failure_rate);
        while (time < duration)
        {
            //  Find time step
            f32 dt = (f32) find_next_failure(rng, system_failure_rate);
            f32 next_t = time + dt;
            if (next_t > duration)
            {
                dt = duration - time;
                time = duration;
                total_throughput += throughput * dt;
                //  Simulation is done
                break;
            }

            total_throughput += throughput * dt;

            //  Find which failure occurs next
            u32 fail_idx = -1;
            f32 failed_so_far = 0.0f;
            const f32 fail_measure = (f32) rmod_msws_rngf(rng) * system_failure_rate;
            for (u32 i = 0; i < node_count; ++i)
            {
                if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
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
            assert(fail_idx < node_count);
            assert(fail_idx != -1);
            const rmod_failure_type type = failure_type[fail_idx];
            node_status[fail_idx] = RMOD_ELEMENT_STATUS_DOWN;
            fails_per_component[fail_idx] += 1;
            if (type == RMOD_FAILURE_TYPE_FATAL)
            {
                break;
            }

            if (type == RMOD_FAILURE_TYPE_ACCEPTABLE)
            {
                //  Deactivate all components which provide/depend only on this one, since there is no point in having
                //  them running while the only part they depend on/provide for is not online
                //  Failure (may) be acceptable

                u32 working_parents = 0;
                // Check for all nodes after the failed node if they should be switched off
                for (u32 i = fail_idx + 1; i < node_count; ++i)
                {
                    //  Is the node even working
                    if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        continue;
                    }
                    //  Count how many of its parents are still working
                    for (u32 j = 0; j < parent_count[i]; ++j)
                    {
                        working_parents += node_status[parent_ids[i][j]] == RMOD_ELEMENT_STATUS_WORK;
                    }
                    if (working_parents == 0)
                    {
                        //  No more working parents, switch it off
                        node_status[i] = RMOD_ELEMENT_STATUS_INACTIVE;
                    }
                }
                if (!working_parents)
                {
                    //  Last node in the graph has no more working parents
                    goto repair;
                }

                u32 working_children = 0;
                //  Check for all nodes before the failed node if they should be switched off
                for (u32 i = fail_idx; i > 0; --i)
                {
                    //  Is the node even working
                    if (node_status[i - 1] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        continue;
                    }
                    //  Count how many of its children are still working
                    for (u32 j = 0; j < child_count[i - 1]; ++j)
                    {
                        working_children += node_status[child_ids[i - 1][j]] == RMOD_ELEMENT_STATUS_WORK;
                    }
                    if (working_children == 0)
                    {
                        //  No more working parents, switch it off
                        node_status[i - 1] = RMOD_ELEMENT_STATUS_INACTIVE;
                    }
                }
                if (!working_children)
                {
                    //  First node in the graph has no more working children
                    //  Literally should not happen
                    assert(false);
                }
                throughput = find_system_throughput(&system_failure_rate, node_count, node_status, parent_count, parent_ids, effect, value, failure_rate);
            }

            if (throughput < repair_limit || type == RMOD_FAILURE_TYPE_CRITICAL)
            {
            repair:
                //  Need to maintain, so repair all systems
                for (u32 i = 0; i < node_count; ++i)
                {
                    if (node_status[i] != RMOD_ELEMENT_STATUS_WORK)
                    {
                        total_cost += cost[i];
                        node_status[i] = RMOD_ELEMENT_STATUS_WORK;
                    }
                }
                //  Restore system to previously computed failure rates
                throughput = full_throughput;
                system_failure_rate = full_fail_rate;
                maintenance_count += 1;
            }

            time = next_t;
        }

        results.total_flow += total_throughput;
        results.total_maintenance_visits += maintenance_count;
        results.sim_count += 1;
        results.total_costs += total_cost;
    }
    *pi_sim = sim_i;
    results.failures_per_component = fails_per_component;
    results.duration = duration;
    results.sim_count = reps_to_do;
    results.n_components = node_count;

    *p_res_out = results;

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

static void* simulate_graph_worker_wrapper(void* param)
{
    const graph_worker_information* worker_info = param;
    char thrd_name[32];
    snprintf(thrd_name, sizeof(thrd_name), "rmod-worker-trhd-%02u", worker_info->worker_thread_id);
    rmod_error_init_thread(thrd_name,
#ifndef NDEBUG
                            RMOD_ERROR_LEVEL_NONE,
#else
                            RMOD_ERROR_LEVEL_WARN,
#endif
                           4,16);
    RMOD_ENTER_FUNCTION;
    rmod_result res = simulate_worker(worker_info->sim_params, worker_info->p_rng_state, worker_info->p_reps_done, worker_info->value, worker_info->node_status, worker_info->fails_per_component, worker_info->p_sim_results);
    if (res != RMOD_RESULT_SUCCESS)
    {
        RMOD_ERROR("Worker simulation thread failed, reason: %s", rmod_result_str(res));
    }
    RMOD_LEAVE_FUNCTION;
    rmod_error_cleanup_thread();
    return NULL;
}

rmod_result rmod_simulate_graph_mt(
        const rmod_graph* graph, f32 simulation_duration, u32 simulation_repetitions, rmod_sim_result* p_res_out,
        u32 thread_count, f32 repair_limit)
{
    RMOD_ENTER_FUNCTION;
    void* const base = lin_jalloc_get_current(G_LIN_JALLOCATOR);
    rmod_result res = RMOD_RESULT_SUCCESS;

    //  Setup worker parameters and work
    simulation_parameters sim_params = {};
    sim_params.reps_to_do = simulation_repetitions / thread_count;
    if (simulation_repetitions % thread_count)
    {
        RMOD_WARN(
                "Simulation repetition number (%u) to perform is not divisible by the number of threads specified (%u). As a consequence a total of %u repetitions will be simulated, which is %u less than expected",
                simulation_repetitions, thread_count, sim_params.reps_to_do * thread_count,
                simulation_repetitions % thread_count);
        simulation_repetitions = sim_params.reps_to_do * thread_count;
    }
    sim_params.duration = simulation_duration;
    sim_params.repair_limit = repair_limit;

    //      Reliability of each element
    const u32 node_count = graph->node_count;
    f32* const failure_rate = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_rate) * node_count);
    if (!failure_rate)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_rate) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Cost of each element
    f32* const cost = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*cost) * node_count);
    if (!cost)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*cost) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Effect of each element
    f32* const effect = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*effect) * node_count);
    if (!effect)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*effect) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Failure type of each element
    rmod_failure_type* const failure_type = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*failure_type) * node_count);
    if (!failure_type)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*failure_type) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Parent counts of each element
    u32* const parent_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_count) * node_count);
    if (!parent_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_count) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Parent arrays of each element
    const rmod_graph_node_id** const parent_ids = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*parent_ids) * node_count);
    if (!parent_ids)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*parent_ids) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Child counts of each element
    u32* const child_count = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*child_count) * node_count);
    if (!child_count)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*child_count) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Child arrays of each element
    const rmod_graph_node_id** const child_ids = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*child_ids) * node_count);
    if (!child_ids)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*child_ids) * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //      Prepare invariant values (failure rates, cost, effect, failure, parent counts, and parent arrays)
    for (u32 i = 0; i < node_count; ++i)
    {
        const rmod_graph_node* node = graph->node_list + i;
        const rmod_graph_node_type* type = graph->type_list + node->type_id;
        failure_rate[i] = type->failure_rate;
        cost[i] = type->cost;
        effect[i] = type->effect;
        failure_type[i] = type->failure_type;
        parent_count[i] = node->parent_count;
        parent_ids[i] = node->parents;
        child_count[i] = node->child_count;
        child_ids[i] = node->children;
    }

    sim_params.node_count = node_count;
    sim_params.failure_rate = failure_rate;
    sim_params.effect = effect;
    sim_params.parent_count = parent_count;
    sim_params.parent_ids = parent_ids;
    sim_params.child_count = child_count;
    sim_params.child_ids = child_ids;
    sim_params.failure_type = failure_type;
    sim_params.cost = cost;


    //  Create workers and send them to work
    graph_worker_information* const worker_information = lin_jalloc(
            G_LIN_JALLOCATOR, sizeof(*worker_information) * thread_count);
    if (!worker_information)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*worker_information) * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    u32* const reps_done_by_workers = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*reps_done_by_workers) * thread_count);
    if (!reps_done_by_workers)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*reps_done_by_workers) * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    memset(reps_done_by_workers, 0, sizeof(*reps_done_by_workers) * thread_count);

    rmod_msws_state* const rng_states_by_thread = lin_jalloc(
            G_LIN_JALLOCATOR, sizeof(*rng_states_by_thread) * thread_count);
    if (!rng_states_by_thread)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*rng_states_by_thread) * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    rmod_msws_state base_rng_state;
    rmod_msws_init(&base_rng_state, 0, 0, 0, 0);

    rmod_sim_result* const result_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*result_array) * thread_count);
    if (!result_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*result_array) * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    memset(result_array, 0, sizeof(*result_array) * thread_count);

    f32* const value_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*value_array) * node_count * thread_count);
    if (!value_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*value_array) * node_count * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    rmod_element_status* const element_status_array = lin_jalloc(
            G_LIN_JALLOCATOR, sizeof(*element_status_array) * node_count * thread_count);
    if (!element_status_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR,
                   sizeof(*element_status_array) * node_count * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    u64* const fails_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*fails_array) * node_count * thread_count);
    if (!fails_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*fails_array) * node_count * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    memset(fails_array, 0, sizeof(*fails_array) * node_count * thread_count);

    for (u32 i = 0; i < thread_count; ++i)
    {
        graph_worker_information* const info_ptr = worker_information + i;
        info_ptr->sim_params = &sim_params;
        info_ptr->worker_thread_id = i;
        info_ptr->p_reps_done = reps_done_by_workers + i;
        info_ptr->p_rng_state = rng_states_by_thread + i;
        //  Use the rng to initialize the rng
        rmod_msws_init(
                rng_states_by_thread + i, rmod_msws_rng(&base_rng_state), rmod_msws_rng(&base_rng_state),
                rmod_msws_rng(&base_rng_state), rmod_msws_rng(&base_rng_state));
        info_ptr->p_sim_results = result_array + i;
        info_ptr->value = value_array + node_count * i;
        info_ptr->value[0] = effect[0];
        info_ptr->node_status = element_status_array + node_count * i;
        info_ptr->fails_per_component = fails_array + node_count * i;
    }
    pthread_t* const worker_id_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*worker_id_array) * thread_count);
    if (!worker_id_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR,
                   sizeof(*worker_id_array) * node_count * thread_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    memset(worker_id_array, 0, sizeof(*worker_id_array) * thread_count);



    f32 full_fail_rate = 0.0f;
    value_array[0] = effect[0];
    f32 full_throughput;
    //  Reset status before simulation
    for (u32 i = 0; i < node_count; ++i)
    {
        element_status_array[i] = RMOD_ELEMENT_STATUS_WORK;
    }
    if ((full_throughput = find_system_throughput(&full_fail_rate, node_count, element_status_array, parent_count, parent_ids, effect, value_array, failure_rate)) < repair_limit)
    {
        RMOD_ERROR("Maximum graph throughput was %g, however minimum value for repair was specified to be %g. This is most likely due to incorrect specification", full_throughput, repair_limit);
        res = RMOD_RESULT_BAD_VALUE;
        goto failed;
    }

    sim_params.full_fail_rate = full_fail_rate;
    sim_params.full_throughput = full_throughput;

#ifndef _WIN32
    struct timespec t_begin;
    int time_res = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t_begin);
    assert(time_res >= 0);
#else
    LARGE_INTEGER t_begin;
    QueryPerformanceCounter(&t_begin);
#endif

    for (u32 i = 0; i < thread_count; ++i)
    {
        pthread_t id;
        const int create_result = pthread_create(&id, NULL, simulate_graph_worker_wrapper, worker_information + i);
        if (create_result < 0)
        {
            RMOD_ERROR("Failed creating worker thread number %u, reason: %s", i, strerror(create_result));
            for (u32 j = 0; j < thread_count; ++j)
            {
                pthread_cancel(worker_id_array[i]);
            }
            res = RMOD_RESULT_BAD_THRD;
            goto failed;
        }
        worker_id_array[i] = id;
    }

    //  Wait for workers and report their progress
    report_sim_progress(0.0, 40);
    u32 sim_reps_total;
    struct timespec sleep = {.tv_nsec = 100000000}; //    100 milliseconds
    do
    {
        sim_reps_total = 0;
        //  Find total number of reps done so far
        sim_reps_total = 0;
        for (u32 i = 0; i < thread_count; ++i)
        {
            sim_reps_total += reps_done_by_workers[i];
        }
        report_sim_progress((f64) sim_reps_total / simulation_repetitions, 40);
        nanosleep(&sleep, NULL);
    } while (sim_reps_total < simulation_repetitions);


#ifndef _WIN32
    struct timespec t_end;
    time_res = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t_end);
    assert(time_res >= 0);
#else
    LARGE_INTEGER t_end, freq;
    QueryPerformanceCounter(&t_end);
    QueryPerformanceFrequency(&freq);
#endif
    //  Collect finished workers
    for (u32 i = 0; i < thread_count; ++i)
    {
        pthread_join(worker_id_array[i], NULL);
    }
    lin_jfree(G_LIN_JALLOCATOR, worker_id_array);

    //  Merge results
    u64* const final_failures = jalloc(sizeof*final_failures * node_count);
    if (!final_failures)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof*final_failures * node_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    rmod_sim_result final_results =
            {
            .sim_count = simulation_repetitions,
            .n_components = node_count,

            .total_flow = 0,
            .total_costs = 0,
            .total_maintenance_visits = 0,

            .failures_per_component = final_failures,
#ifndef _WIN32
            .duration = (f32)((f64)(t_end.tv_sec - t_begin.tv_sec) + ((f64)(t_end.tv_nsec - t_begin.tv_nsec)) / 1e9),
#else

            .duration = (f32)((f64)(t_end.QuadPart - t_begin.QuadPart) / (f64)freq.QuadPart),
#endif
            };
    memset(final_failures, 0, sizeof*final_failures * node_count);
    for (u32 i = 0; i < thread_count; ++i)
    {
        rmod_sim_result* const thrd_res = result_array + i;
        for (u32 j = 0; j < node_count; ++j)
        {
            final_failures[j] += thrd_res->failures_per_component[j];
        }
        final_results.total_maintenance_visits += thrd_res->total_maintenance_visits;
        final_results.total_costs += thrd_res->total_costs;
        final_results.total_flow += thrd_res->total_flow;
    }
    final_results.max_flow = full_throughput;
    fprintf(stdout, "\nSimulation took %g seconds of CPU time\n", final_results.duration);

    lin_jfree(G_LIN_JALLOCATOR, fails_array);
    lin_jfree(G_LIN_JALLOCATOR, element_status_array);
    lin_jfree(G_LIN_JALLOCATOR, value_array);
    lin_jfree(G_LIN_JALLOCATOR, result_array);
    lin_jfree(G_LIN_JALLOCATOR, rng_states_by_thread);
    lin_jfree(G_LIN_JALLOCATOR, reps_done_by_workers);
    lin_jfree(G_LIN_JALLOCATOR, worker_information);
    lin_jfree(G_LIN_JALLOCATOR, child_ids);
    lin_jfree(G_LIN_JALLOCATOR, child_count);
    lin_jfree(G_LIN_JALLOCATOR, parent_ids);
    lin_jfree(G_LIN_JALLOCATOR, parent_count);
    lin_jfree(G_LIN_JALLOCATOR, failure_type);
    lin_jfree(G_LIN_JALLOCATOR, effect);
    lin_jfree(G_LIN_JALLOCATOR, cost);
    lin_jfree(G_LIN_JALLOCATOR, failure_rate);
    //  Clean up

    //  Return finished results
    *p_res_out = final_results;

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
failed:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base);
    RMOD_LEAVE_FUNCTION;
    return res;
}
