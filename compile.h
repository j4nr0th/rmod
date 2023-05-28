//
// Created by jan on 28.5.2023.
//

#ifndef RMOD_COMPILE_H
#define RMOD_COMPILE_H
#include "graph_parsing.h"


typedef uint_fast32_t rmod_graph_node_id, rmod_graph_type_id;

typedef struct rmod_graph_struct rmod_graph;
struct rmod_graph_struct
{
    //  Graph information
    c8* module_name;                        //  Should probably set as file name
    c8* graph_type;                         //  Should be the chain's name

    //  Node information
    uint_fast32_t node_count;               //  Number of nodes in the graph
    rmod_graph_type_id* type_list;               //  Array of types of individual nodes
    u32* child_count_list;                  //  Array of child counts of individual nodes
    rmod_graph_node_id** child_array_list;       //  Array of arrays with indices of children
    u32* parent_count_list;                 //  Array of parent counts of individual nodes
    rmod_graph_node_id** parent_array_list;      //  Array of arrays with indices of parents

    //  Type information
    uint_fast32_t type_count;               //  Number of node types
    c8** type_names;                        //  Array of type names
    f32* reliability_list;                  //  Array of type reliabilities
    f32* effect_list;                       //  Array of type effects
    rmod_failure_type* failure_type_list;   //  Array of type failure types
};


rmod_result rmod_compile_graph(u32 n_types, const rmod_element_type* p_types, const char* chain_name, rmod_graph* p_out);

rmod_result rmod_destroy_graph(rmod_graph* graph);

#endif //RMOD_COMPILE_H
