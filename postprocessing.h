//
// Created by jan on 10.6.2023.
//

#ifndef RMOD_POSTPROCESSING_H
#define RMOD_POSTPROCESSING_H
#include "rmod.h"
#include "simulation_run.h"
#include "fmt/sstream.h"
#include "compile.h"
#include "program.h"

rmod_result rmod_postprocess_results(
        const rmod_sim_result* results, f64 sim_duration, f64 repair_limit, const rmod_graph* graph, u32 thread_count,
        const rmod_program* program, int argc, const char* argv[], string_stream* sstream);

#endif //RMOD_POSTPROCESSING_H
