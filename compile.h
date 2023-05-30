//
// Created by jan on 28.5.2023.
//

#ifndef RMOD_COMPILE_H
#define RMOD_COMPILE_H
#include "graph_parsing.h"


typedef uint_fast32_t rmod_graph_node_id, rmod_graph_type_id;
typedef struct rmod_graph_node_struct rmod_graph_node;
struct rmod_graph_node_struct
{
    rmod_graph_type_id type_id;
    u32 child_count;
    rmod_graph_node_id* children;
    u32 parent_count;
    rmod_graph_node_id* parents;
};
typedef struct rmod_graph_node_type_struct rmod_graph_node_type;
struct rmod_graph_node_type_struct
{
    c8* name;
    f32 reliability;
    f32 effect;
    rmod_failure_type failure_type;
};

typedef struct rmod_graph_struct rmod_graph;
struct rmod_graph_struct
{
    //  Graph information
    c8* module_name;                        //  Should probably set as file name
    c8* graph_type;                         //  Should be the chain's name

    //  Node information
    uint_fast32_t node_count;               //  Number of nodes
    rmod_graph_node* node_list;             //  Array of nodes

    //  Type information
    uint_fast32_t type_count;               //  Number of node types
    rmod_graph_node_type* type_list;        //  Array of node types
};


rmod_result rmod_compile_graph(
        u32 n_types, rmod_element_type* p_types, const char* chain_name, const char* module_name,
        rmod_graph* p_out);

rmod_result rmod_destroy_graph(rmod_graph* graph);

rmod_result rmod_decompile_graph(rmod_graph* graph, u32* pn_types, rmod_element_type** pp_types);

#endif //RMOD_COMPILE_H
