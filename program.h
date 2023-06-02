//
// Created by jan on 31.5.2023.
//

#ifndef RMOD_PROGRAM_H
#define RMOD_PROGRAM_H
#include "graph_parsing.h"
#include "compile.h"

typedef struct rmod_program_struct rmod_program;
struct rmod_program_struct
{
    u32 n_types;
    rmod_element_type* p_types;
    u32 n_files;
    rmod_memory_file* mem_files;
    xml_element xml_root;
};

rmod_result rmod_program_create(const char* file_name, rmod_program* p_program);

rmod_result rmod_program_delete(rmod_program* program);

rmod_result
rmod_compile(const rmod_program* program, rmod_graph* graph, const char* chain_name, const char* module_name);

#endif //RMOD_PROGRAM_H