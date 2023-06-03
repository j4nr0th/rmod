//
// Created by jan on 2.6.2023.
//

#ifndef RMOD_SIMULATION_RUN_H
#define RMOD_SIMULATION_RUN_H
#include "rmod.h"
#include "compile.h"
#include "random/acorn.h"

typedef enum rmod_element_status_enum rmod_element_status;
enum rmod_element_status_enum
{
    RMOD_ELEMENT_STATUS_DOWN = 0,
    RMOD_ELEMENT_STATUS_WORK = 1,
};

typedef struct rmod_sim_result_struct rmod_sim_result;
struct rmod_sim_result_struct
{
    u32 sim_count;
    f32 avg_flow;
    f32 avg_fail_count;
};

rmod_result rmod_simulate_graph(rmod_acorn_state* acorn, const rmod_graph* graph, f32 sim_time, u32 sim_times, rmod_sim_result* p_res_out);

#endif //RMOD_SIMULATION_RUN_H