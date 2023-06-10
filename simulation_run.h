//
// Created by jan on 2.6.2023.
//

#ifndef RMOD_SIMULATION_RUN_H
#define RMOD_SIMULATION_RUN_H
#include "rmod.h"
#include "compile.h"
//#include "random/acorn.h"

typedef enum rmod_element_status_enum rmod_element_status;
enum rmod_element_status_enum
{
    RMOD_ELEMENT_STATUS_DOWN = 0,       //  Component has broken down and needs repair
    RMOD_ELEMENT_STATUS_WORK = 1,       //  Component is normal-sized and fully functional
    RMOD_ELEMENT_STATUS_INACTIVE = 2,   //  Component is not broken, but is not working, all components that it depends
                                        //  on or which it provides for are not working anymore
};

typedef struct rmod_sim_result_struct rmod_sim_result;
struct rmod_sim_result_struct
{
    u64 sim_count;
    f32 total_flow;
    u64 total_maintenance_visits;
    f32 total_costs;
    f32 duration;
    u32 n_components;
    u64* failures_per_component;
};

rmod_result rmod_simulate_graph(
        const rmod_graph* graph, f32 simulation_duration, u32 simulation_repetitions, rmod_sim_result* p_res_out,
        f64 (* rng_function)(void*), void* rng_param, f32 repair_limit);

rmod_result rmod_simulate_graph_mt(
        const rmod_graph* graph, f32 simulation_duration, u32 simulation_repetitions, rmod_sim_result* p_res_out,
        u32 thread_count, f32 repair_limit);

#endif //RMOD_SIMULATION_RUN_H
