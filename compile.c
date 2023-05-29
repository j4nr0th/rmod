//
// Created by jan on 28.5.2023.
//

#include "compile.h"

typedef struct chain_dependency_info_struct chain_dependency_info;
struct chain_dependency_info_struct
{
    u32 index;
    u32 dependency_count;
    u32* dependency_list;
};

static rmod_result check_for_cycles(const u32 n_info, const chain_dependency_info* const info_list, const u32 index, const u32 depth, const u32 max_depth)
{
    if (depth > max_depth)
    {
        return RMOD_RESULT_CYCLICAL_CHAIN_DEPENDENCY;
    }
    const chain_dependency_info* info = info_list + index;
    for (u32 i = 0; i < info->dependency_count; ++i)
    {
        rmod_result res = check_for_cycles(n_info, info_list, info->dependency_list[i], depth + 1, max_depth);
        if (res != RMOD_RESULT_SUCCESS)
        {
            return res;
        }
    }
    return RMOD_RESULT_SUCCESS;
}

static u32 compute_dependants_counts(const chain_dependency_info* const info_list, const u32 index, u32* const count_array)
{
    const chain_dependency_info* info = info_list + index;
    //  Add direct dependencies
    count_array[index] += info->dependency_count;
    //  Call recursively for all children
    u32 child_dependency_total = 0;
    for (u32 i = 0; i < info->dependency_count; ++i)
    {
        child_dependency_total += compute_dependants_counts(info_list, info->dependency_list[i], count_array);
    }
    //  Add dependency counts of children
    count_array[index] += child_dependency_total;
    return info->dependency_count + child_dependency_total;
}

rmod_result rmod_compile_graph(
        u32 n_types, const rmod_element_type* p_types, const char* chain_name, const char* module_name,
        rmod_graph* p_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* const base_lin_alloc = lin_jalloc_get_current(G_LIN_JALLOCATOR);

    //  Search for proper chain
    u32 chain_count = 0;
    const rmod_chain* chain = NULL;
    for (u32 i = 0; i < n_types; ++i)
    {
        const bool is_chain = p_types[i].type.type == RMOD_ELEMENT_TYPE_CHAIN;
        if (is_chain && strncmp(chain_name, p_types[i].type.type_name.begin, p_types[i].type.type_name.len) == 0)
        {
            assert(!chain);
            chain = &p_types[i].chain;
        }
        chain_count += is_chain;
    }
    if (!chain)
    {
        RMOD_ERROR("Could not find chain named \"%s\"", chain_name);
        res = RMOD_RESULT_BAD_CHAIN_NAME;
        goto failed;
    }

    //  Build chain hierarchy and ensure there are no cycles
    u32* chain_index_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*chain_index_array) * chain_count);
    if (!chain_index_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof* chain_index_array * chain_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    u32* build_order_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*build_order_array) * chain_count);
    if (!build_order_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*build_order_array) * chain_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    rmod_graph* chain_build_array  = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*chain_build_array) * chain_count);
    if (!chain_build_array)
    {
        RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*chain_build_array) * chain_count);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }

    //  Find the proper build order
    {
        chain_dependency_info* dependency_info_list = lin_jalloc(
                G_LIN_JALLOCATOR, sizeof *dependency_info_list * chain_count);
        if (!dependency_info_list)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof *dependency_info_list * chain_count);
            res = RMOD_RESULT_NOMEM;
            goto failed;
        }
        u32* joined_dependency_array = lin_jalloc(
                G_LIN_JALLOCATOR, sizeof *joined_dependency_array * chain_count * chain_count);
        if (!joined_dependency_array)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR,
                       sizeof *joined_dependency_array * chain_count * chain_count);
            res = RMOD_RESULT_NOMEM;
            goto failed;
        }
        u32 chain_idx = -1;
        //  Count the numer of chains present and what is the order of the target chain
        for (u32 i = 0, j = 0; i < n_types && j < chain_count; ++i)
        {
            //  Is this type a chain
            if (p_types[i].type.type != RMOD_ELEMENT_TYPE_CHAIN)
            {
                continue;
            }
            if (&p_types[i].chain == chain)
            {
                chain_idx = j;
            }
            chain_index_array[j++] = i;
        }
        assert(chain_idx != -1);

        //  For each chain, loop through all elements and identify which of them are chains,
        //  then add them to the list of chain dependencies for that chain (check that there
        //  are no duplicates).
        for (u32 i = 0, j = 0; i < n_types && j < chain_count; ++i)
        {
            //  Is this type a chain
            if (p_types[i].type.type != RMOD_ELEMENT_TYPE_CHAIN)
            {
                continue;
            }
            chain_dependency_info* const info = dependency_info_list + j;
            info->index = j;
            info->dependency_count = 0;
            info->dependency_list = joined_dependency_array + chain_count * j;

            const rmod_chain* this = &p_types[i].chain;
            //  Go over elements to discover which ones are chains
            for (u32 k = 0; k < this->element_count; ++k)
            {
                const rmod_chain_element* element = this->chain_elements + k;
                const rmod_element_type* element_type = p_types + element->type_id;
                //  Is the element's type chain?
                if (element_type->type.type != RMOD_ELEMENT_TYPE_CHAIN)
                {
                    continue;
                }
                //  Translate the position in type array in position in chain array
                u32 l, pos;
                for (pos = 0; pos < chain_count; ++pos)
                {
                    if (chain_index_array[pos] == element->type_id)
                    {
                        break;
                    }
                }
                assert(pos < chain_count);
                //  Check for duplicate dependency
                for (l = 0; l < info->dependency_count; ++l)
                {
                    if (info->dependency_list[l] == pos)
                    {
                        //  Already in there
                        break;
                    }
                }
                if (l == info->dependency_count)
                {
                    info->dependency_list[info->dependency_count++] = pos;
                }
                assert(info->dependency_count <= chain_count);
            }

            j += 1;
        }

        //  Extract the dependencies needed by the target chain
        chain_dependency_info* const needed_dependencies = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*needed_dependencies) * chain_count);
        if (!needed_dependencies)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*needed_dependencies) * chain_count);
            res = RMOD_RESULT_NOMEM;
            goto failed;
        }
        u32* const queue = lin_jalloc(G_LIN_JALLOCATOR, sizeof*queue * chain_count);
        if (!queue)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sizeof(*queue) * chain_count);
            res = RMOD_RESULT_NOMEM;
            goto failed;
        }
        u32 queue_start = 0;
        u32 queue_count = 0;


        u32 needed_count = 1;
        needed_dependencies[0] = dependency_info_list[chain_idx];
        for (u32 i = 0; i < needed_count; ++i)
        {
            assert(i < chain_count);
            //  For each chain in the needed array check whether all of its dependencies are already
            //  in the array and add them if not
            for (u32 j = 0; j < needed_dependencies[i].dependency_count; ++j)
            {
                const u32 dependency = needed_dependencies->dependency_list[j];
                u32 k;
                for (k = 0; k < needed_count; ++k)
                {
                    if (dependency == needed_dependencies[k].index)
                    {
                        break;
                    }
                }
                if (k == needed_count)
                {
                    needed_dependencies[k] = dependency_info_list[k];
                    needed_count += 1;
                }
            }
        }

        //  Now all the actually needed dependencies reside in the needed_dependencies array with the length of
        //  needed_count. Next step is performing a topological sort of the array.
        u32 left_to_sort = needed_count;
        u32 sorted_count = 0;
        while (left_to_sort)
        {
            //  Extract the nodes with no dependencies
            for (u32 i = 0; i < left_to_sort; ++i)
            {
                if (needed_dependencies[i].dependency_count == 0)
                {
                    //  Take it out of the array and move it into the queue
                    u32 idx = needed_dependencies[i].index;
                    queue[queue_start + queue_count++] = idx;
                    memmove(needed_dependencies + i, needed_dependencies + i + 1, sizeof(*needed_dependencies) * (left_to_sort - 1 - i));
                    memset(needed_dependencies + left_to_sort, 0, sizeof(*needed_dependencies));
                    left_to_sort -= 1;
                    i -= 1;

                    //  Remove dependence on this from the other chains
                    for (u32 j = 0; j < left_to_sort; ++j)
                    {
                        for (u32 k = 0; k < needed_dependencies[j].dependency_count; ++k)
                        {
                            if (needed_dependencies[j].dependency_list[k] == idx)
                            {
                                memmove(needed_dependencies[j].dependency_list + k, needed_dependencies[j].dependency_list + k + 1, sizeof(*needed_dependencies[j].dependency_list) * (needed_dependencies[j].dependency_count - 1 - k));
                                needed_dependencies[j].dependency_count -= 1;
                                break;
                            }
                        }
                    }
                }
            }

            //  Remove the extracted values and push them in the sorted array

            //      If queue is empty, there is at least one cycle in the chain dependencies
            if (queue_count == 0)
            {
                RMOD_ERROR("Cyclical dependencies for chain \"%s\" were found");
                res = RMOD_RESULT_CYCLICAL_CHAIN_DEPENDENCY;
                goto failed;
            }

            for (u32 i = 0; i < queue_count; ++i)
            {
                build_order_array[sorted_count++] = queue[queue_start + i];
            }
            queue_count = 0;

            assert(sorted_count + left_to_sort == needed_count);
        }



        lin_jfree(G_LIN_JALLOCATOR, queue);
        lin_jfree(G_LIN_JALLOCATOR, needed_dependencies);
        lin_jfree(G_LIN_JALLOCATOR, joined_dependency_array);
        lin_jfree(G_LIN_JALLOCATOR, dependency_info_list);
    }

    //  chain_index_array has the indices of the chains within the p_types
    //  build_order_array has the order in which the chains have to be built (0 corresponding to the first element in chain_index_array)
    //  chain_build_array has space where built chains can be inserted into

    //  Compile chains based on their topological ordering (nice fancy words)


    lin_jfree(G_LIN_JALLOCATOR, chain_build_array);
    lin_jfree(G_LIN_JALLOCATOR, build_order_array);
    lin_jfree(G_LIN_JALLOCATOR, chain_index_array);

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;

failed:
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base_lin_alloc);
    RMOD_LEAVE_FUNCTION;
    return res;
}
