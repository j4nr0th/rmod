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

static rmod_result copy_element(const rmod_chain_element* const this, rmod_chain_element* const dest)
{
    RMOD_ENTER_FUNCTION;
    ptrdiff_t* new_parents, *new_children;
    if (this->parent_count)
    {
        new_parents = jalloc(this->parent_count * sizeof(*new_parents));
        if (!new_parents)
        {
            RMOD_ERROR("Failed jalloc(%zu)",this->parent_count * sizeof(*new_parents));
            RMOD_LEAVE_FUNCTION;
            return RMOD_RESULT_NOMEM;
        }
    }
    else
    {
        new_parents = NULL;
    }
    if (this->child_count)
    {
        new_children = jalloc(this->child_count * sizeof(*new_children));
        if (!new_children)
        {
            jfree(new_parents);
            RMOD_ERROR("Failed jalloc(%zu)", this->parent_count * sizeof(*new_children));
            RMOD_LEAVE_FUNCTION;
            return RMOD_RESULT_NOMEM;
        }
    }
    else
    {
        new_children = NULL;
    }
    static_assert(sizeof(*dest) == sizeof(*this));
    memcpy(dest, this, sizeof(*this));
    dest->parents = new_parents;
    if (this->parent_count)
    {
        memcpy(new_parents, this->parents, this->parent_count * sizeof(*new_parents));
    }
    dest->children = new_children;
    if (this->child_count)
    {
        memcpy(new_children, this->children, this->child_count * sizeof(*new_children));
    }
    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

static inline void chain_to_absolute_relations(rmod_chain* const chain)
{
    for (u32 i = 0; i < chain->element_count; ++i)
    {
        rmod_chain_element* const this = chain->chain_elements + i;
        for (u32 j = 0; j < this->parent_count; ++j)
        {
            this->parents[j] += i;
        }
        for (u32 j = 0; j < this->child_count; ++j)
        {
            this->children[j] += i;
        }
    }
}

static inline void chain_to_relative_relations(rmod_chain* const chain)
{
    for (u32 i = 0; i < chain->element_count; ++i)
    {
        rmod_chain_element* const this = chain->chain_elements + i;
        for (u32 j = 0; j < this->parent_count; ++j)
        {
            this->parents[j] -= i;
        }
        for (u32 j = 0; j < this->child_count; ++j)
        {
            this->children[j] -= i;
        }
    }
}

rmod_result rmod_compile_graph(
        u32 n_types, rmod_element_type* p_types, const char* chain_name, const char* module_name,
        rmod_graph* p_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    void* const base_lin_alloc = lin_jalloc_get_current(G_LIN_JALLOCATOR);

    //  Search for proper chain
    u32 chain_count = 0;
    u32 total_chain_name_memory = 0;
    const rmod_chain* target_chain = NULL;
    for (u32 i = 0; i < n_types; ++i)
    {
        const bool is_chain = p_types[i].header.type_value == RMOD_ELEMENT_TYPE_CHAIN;
        if (is_chain && strncmp(chain_name, p_types[i].header.type_name.begin, p_types[i].header.type_name.len) == 0)
        {
            assert(!target_chain);
            target_chain = &p_types[i].chain;
        }
        chain_count += is_chain;
        total_chain_name_memory += p_types[i].header.type_name.len + 1;
    }
    if (!target_chain)
    {
        RMOD_ERROR("Could not find chain named \"%s\"", chain_name);
        res = RMOD_RESULT_BAD_CHAIN_NAME;
        goto failed;
    }
    if (n_types == chain_count)
    {
        RMOD_ERROR("All types passed were chains. HOW IN GOD'S NAME DO YOU EXPECT TO BUILD A RELIABILITY BLOCK DIAGRAM WITH NO BLOCKS!?");
        res = RMOD_RESULT_STUPIDITY;
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
    u32 needed_count = 1;

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
            if (p_types[i].header.type_value != RMOD_ELEMENT_TYPE_CHAIN)
            {
                continue;
            }
            if (&p_types[i].chain == target_chain)
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
            if (p_types[i].header.type_value != RMOD_ELEMENT_TYPE_CHAIN)
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
                if (element_type->header.type_value != RMOD_ELEMENT_TYPE_CHAIN)
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
        u32 queue_count = 0;


        needed_dependencies[0] = dependency_info_list[chain_idx];
        for (u32 i = 0; i < needed_count; ++i)
        {
            assert(i < chain_count);
            //  For each chain in the needed array check whether all of its dependencies are already
            //  in the array and add them if not
            for (u32 j = 0; j < needed_dependencies[i].dependency_count; ++j)
            {
                const u32 dependency = needed_dependencies[i].dependency_list[j];
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
                    needed_dependencies[k] = dependency_info_list[dependency];
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
                    queue[queue_count++] = idx;
                    memmove(needed_dependencies + i, needed_dependencies + i + 1, sizeof(*needed_dependencies) * (left_to_sort - 1 - i));
                    memset(needed_dependencies + left_to_sort - 1, 0, sizeof(*needed_dependencies));
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
                RMOD_ERROR("Cyclical dependencies for chain \"%s\" were found", chain_name);
                res = RMOD_RESULT_CYCLICAL_CHAIN_DEPENDENCY;
                goto failed;
            }

            for (u32 i = 0; i < queue_count; ++i)
            {
                build_order_array[sorted_count++] = queue[i];
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


    //  Compile chains based on their topological ordering (nice fancy words)
    u32 built_count = 0;
    for (u32 i = 0; i < needed_count; ++i, ++built_count)
    {
        const u32 chain_to_build_index = build_order_array[i];
        rmod_chain* const chain = &p_types[chain_index_array[chain_to_build_index]].chain;
        if (chain->compiled)
        {
            //  No compilation needed
            continue;
        }
        assert(chain->header.type_value == RMOD_ELEMENT_TYPE_CHAIN);
        RMOD_INFO("Building chain \"%.*s\"", chain->header.type_name.len, chain->header.type_name.begin);

        //  Building a chain means substituting all of its elements for blocks this requires all of its dependencies to
        //  also be built already

        u32 total_element_count = 0;
        for (u32 j = 0; j < chain->element_count; ++j)
        {
            const rmod_chain_element* element = chain->chain_elements + j;
            const rmod_element_type* element_type = p_types + element->type_id;
            switch (element_type->header.type_value)
            {
            case RMOD_ELEMENT_TYPE_BLOCK:
                total_element_count += 1;
                break;
            case RMOD_ELEMENT_TYPE_CHAIN:
                total_element_count += element_type->chain.element_count;
                break;
            default:RMOD_ERROR("Element type \"%.*s\" had invalid type", element_type->header.type_name.len, element_type->header.type_name.begin);
            res = RMOD_RESULT_BAD_XML;
            goto failed;
            }
        }

        if (total_element_count != chain->element_count)
        {

            rmod_chain_element* new_element_array = jrealloc(chain->chain_elements, total_element_count * sizeof*new_element_array);
            if (!new_element_array)
            {
                RMOD_ERROR("Failed jrealloc(%p, %zu)", chain->chain_elements, total_element_count * sizeof*new_element_array);
                res = RMOD_RESULT_NOMEM;
                goto failed;
            }
            memset(new_element_array + chain->element_count, 0, (total_element_count - chain->element_count) * sizeof*new_element_array);
            chain->chain_elements = new_element_array;
        }

        //  Now performa a topological sort of the elements
        rmod_chain_element* const sorted_array = lin_jalloc(G_LIN_JALLOCATOR, total_element_count * sizeof(*sorted_array));
        if (!sorted_array)
        {
            RMOD_ERROR("Failed lin_jalloc(%p, %g)", G_LIN_JALLOCATOR, total_element_count * sizeof(*sorted_array));
            res = RMOD_RESULT_NOMEM;
            goto failed;
        }
        u32 left_to_sort = chain->element_count;

        //  Convert parent-child relations to be absolute rather than relative
        chain_to_absolute_relations(chain);
        while (left_to_sort)
        {
            u32 found = 0;
            //  Extract the nodes with no dependencies
            for (u32 j = 0; j < left_to_sort; ++j)
            {
                if (chain->chain_elements[j].parent_count == 0)
                {
                    found += 1;
                    //  Take it out of the array and move it into the queue
                    const rmod_chain_element e = chain->chain_elements[j];
                    sorted_array[(chain->element_count - left_to_sort)] = e;
                    memmove(chain->chain_elements + j, chain->chain_elements + j + 1, sizeof(*chain->chain_elements) * (left_to_sort - 1 - j));
                    memset(chain->chain_elements + left_to_sort - 1, 0, sizeof(*chain->chain_elements));
                    left_to_sort -= 1;
                    j -= 1;

                    //  Remove dependence on this from the other chains
                    for (u32 k = 0; k < left_to_sort; ++k)
                    {
                        for (u32 l = 0; l < chain->chain_elements[k].parent_count; ++l)
                        {
                            if (chain->chain_elements[k].parents[l] == e.id)
                            {
                                memmove(chain->chain_elements[k].parents + l, chain->chain_elements[k].parents + l + 1, sizeof(*chain->chain_elements[k].parents) * (chain->chain_elements[k].parent_count - 1 - l));
                                chain->chain_elements[k].parent_count -= 1;
                                break;
                            }
                        }
                    }
                }
            }

            //  Remove the extracted values and push them in the sorted array

            //      If queue is empty, there is at least one cycle in the chain dependencies
            if (found == 0)
            {
                RMOD_ERROR_CRIT("Cyclical flow for chain \"%.*s\" were found", chain->header.type_name.len, chain->header.type_name.begin);
                //  Not really necessary, since this function calls exit
                exit(EXIT_FAILURE);
            }
        }

        //  Update chain first and last indices
        chain->i_first = 0;
        chain->i_last = chain->element_count - 1;

        memmove(chain->chain_elements, sorted_array, sizeof(*sorted_array) * chain->element_count);
        lin_jfree(G_LIN_JALLOCATOR, sorted_array);
        //  Adjust child references
        u32* const ordering_array = lin_jalloc(G_LIN_JALLOCATOR, sizeof(*ordering_array) * chain->element_count);
        assert(ordering_array);
        //  Allocation will succeed because a bigger chunk of memory was just given back
        for (u32 j = 0; j < chain->element_count; ++j)
        {
            ordering_array[j] = chain->chain_elements[j].id;
        }
        for (u32 j = 0; j < chain->element_count; ++j)
        {
            rmod_chain_element* const this = chain->chain_elements + j;
            for (u32 k = 0; k < this->child_count; ++k)
            {
                for (u32 l = 0; l < chain->element_count; ++l)
                {
                    if (ordering_array[l] == this->children[k])
                    {
                        this->children[k] = l;
                        break;
                    }
                }
            }
            this->id = j;
        }


        lin_jfree(G_LIN_JALLOCATOR, ordering_array);


        //  Re-generate child relations by using child references
        for (u32 j = 0; j < chain->element_count; ++j)
        {
            rmod_chain_element* const this = chain->chain_elements + j;
            for (u32 k = 0; k < this->child_count; ++k)
            {
                rmod_chain_element* child = chain->chain_elements + this->children[k];
                child->parents[child->parent_count++] = j;
            }
        }

        //  Reset the chain to use relative offsets
        chain_to_relative_relations(chain);

        if (total_element_count != chain->element_count)
        {
            //  Lastly go through the element list and substitute all the chains. Since all elements contain relative ordering,
            //  this substitution should not need any reordering. Space for all new elements was already reserved before
            for (u32 j = 0; j < chain->element_count; ++j)
            {
                const rmod_chain_element* element = chain->chain_elements + j;
                const rmod_element_type* type = p_types + element->type_id;
                //  Not a chain, so skip
                if (type->header.type_value != RMOD_ELEMENT_TYPE_CHAIN)
                {
                    continue;
                }
                const rmod_chain* sub_chain = &type->chain;
                if (!sub_chain->compiled)
                {
                    RMOD_ERROR("Chain \"%.*s\" was not compiled as dependency for chain \"%.*s\"", sub_chain->header.type_name.len, sub_chain->header.type_name.begin, chain->header.type_name.len, chain->header.type_name.begin);
                    res = RMOD_RESULT_STUPIDITY;
                    goto failed;
                }
                const u32 sub_elements = sub_chain->element_count;
                //  Save element which will be replaced
                const rmod_chain_element e_replaced = chain->chain_elements[j];
                //  Create a copy of elements that will be inserted
                rmod_chain_element* const element_copy = lin_jalloc(G_LIN_JALLOCATOR, sub_elements * sizeof(*element_copy));
                if (!element_copy)
                {
                    RMOD_ERROR("Failed lin_jalloc(%p, %zu)", G_LIN_JALLOCATOR, sub_elements * sizeof(*element_copy));
                    res = RMOD_RESULT_NOMEM;
                    goto failed;
                }
                for (u32 k = 0; k < sub_elements; ++k)
                {
                    if ((res = copy_element(sub_chain->chain_elements + k, element_copy + k)))
                    {
                        RMOD_ERROR("Failed copying element");
                        for (u32 l = 0; l < k; ++l)
                        {
                            jfree(element_copy[l].parents);
                            jfree(element_copy[l].children);
                        }
                        goto failed;
                    }
                    element_copy[k].label.len = 0;
                }

                //  Correct all parent-child relations of the elements proceeding and following this one
                for (u32 k = 0; k < j; ++k)
                {
                    rmod_chain_element* e = chain->chain_elements + k;
                    for (u32 l = 0; l < e->child_count; ++l)
                    {
                        if (e->children[l] + k > j)
                        {
                            e->children[l] += sub_elements - 1;
                        }
                    }
                }
                for (u32 k = j + 1; k < chain->element_count; ++k)
                {
                    rmod_chain_element* e = chain->chain_elements + k;
                    for (u32 l = 0; l < e->parent_count; ++l)
                    {
                        if (e->parents[l] + k < j)
                        {
                            e->parents[l] -= sub_elements - 1;
                        }
                    }
                }
                //  Move other elements out of the way
                memmove(chain->chain_elements + j + sub_elements, chain->chain_elements + j + 1, sizeof(*chain->chain_elements) * (chain->element_count - j - 1));
                assert(element_copy[0].parent_count == 0);
                element_copy[0].parent_count = e_replaced.parent_count;
                assert(element_copy[0].parents == NULL);
                element_copy[0].parents = e_replaced.parents;
                assert(element_copy[sub_elements - 1].child_count == 0);
                element_copy[sub_elements - 1].child_count = e_replaced.child_count;
                assert(element_copy[sub_elements - 1].children == NULL);
                element_copy[sub_elements - 1].children = e_replaced.children;
                //  Copy the memory from the copy_array now that there's space for them
                memcpy(chain->chain_elements + j, element_copy, sizeof(*element_copy) * sub_elements);
                lin_jfree(G_LIN_JALLOCATOR, element_copy);
                chain->element_count += sub_elements - 1;
                j += sub_elements - 1;

                //  Move loop index forward
            }
        }

        assert(chain->element_count == total_element_count);
        chain->i_first = 0;
        chain->i_last = chain->element_count - 1;
        chain->compiled = true;
    }

    lin_jfree(G_LIN_JALLOCATOR, build_order_array);
    lin_jfree(G_LIN_JALLOCATOR, chain_index_array);

    //  Chain compilation is complete. Now the target is compiled.
    assert(target_chain->compiled);
    //  Now convert the chain into a graph
    rmod_graph_node_type* const type_array = jalloc(sizeof*type_array * (n_types - chain_count));
    if (!type_array)
    {
        RMOD_ERROR("Failed jalloc(%zu)", sizeof*type_array * (n_types - chain_count));
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    u32 unique_types = 0;

    rmod_graph this;
    memset(&this, 0xCC, sizeof(this));
    this.node_count = target_chain->element_count;
    this.node_list = jalloc(this.node_count * sizeof*this.node_list);
    if (!this.node_list)
    {
        RMOD_ERROR("Failed jalloc(%zu)", this.node_count * sizeof*this.node_list);
        res = RMOD_RESULT_NOMEM;
        goto failed;
    }
    u32 i;
    for (i = 0; i < target_chain->element_count; ++i)
    {
        const rmod_chain_element* const element = target_chain->chain_elements + i;
        const rmod_element_type* const type = p_types + element->type_id;
        if (type->header.type_value != RMOD_ELEMENT_TYPE_BLOCK)
        {
            RMOD_ERROR("Converting element \"%*.s\" - BLOCK-%03i was not possible because its type was not block. This should have not have happened, since chain was marked as compiled.");
            res = RMOD_RESULT_STUPIDITY;
            goto free_failed;
        }
        //  Add the node's type if needed
        const rmod_block* const block_type = &type->block;
        u32 type_index;
        for (type_index = 0; type_index < unique_types; ++type_index)
        {
            if (strncmp((const char*)type_array[type_index].name, block_type->header.type_name.begin, block_type->header.type_name.len) == 0)
            {
                //  Was already put in there
                break;
            }
        }
        if (type_index == unique_types)
        {
            //  Add it in the array
            c8* name_buffer = jalloc(block_type->header.type_name.len + 1);
            if (!name_buffer)
            {
                RMOD_ERROR("Failed jalloc(%zu)", block_type->header.type_name.len + 1);
                res = RMOD_RESULT_NOMEM;
                goto free_failed;
            }
            memcpy(name_buffer, block_type->header.type_name.begin, block_type->header.type_name.len);
            name_buffer[block_type->header.type_name.len] = 0;
            type_array[unique_types].name = name_buffer;
            type_array[unique_types].failure_type = block_type->failure_type;
            type_array[unique_types].failure_rate = (block_type->mtbf == 0.0) ? INFINITY : 1.0f / block_type->mtbf;
            type_array[unique_types].effect = block_type->effect;
            type_array[unique_types].cost = block_type->cost;
            unique_types += 1;
            assert(unique_types <= n_types - chain_count);
        }

        //  Add the node itself
        rmod_graph_node_id* parents, *children;
        if (element->parent_count)
        {
            parents = jalloc(sizeof*parents * element->parent_count);
            if (!parents)
            {
                RMOD_ERROR("Failed jalloc(%zu)", sizeof*parents * element->parent_count);
                res = RMOD_RESULT_NOMEM;
                goto free_failed;
            }
            //  Convert parent indices to use absolute values, rather than relative
            for (u32 k = 0; k < element->parent_count; ++k)
            {
                assert(element->parents[k] + i >= 0);
                parents[k] = (element_type_id)(element->parents[k] + i);
            }
        }
        else
        {
            parents = NULL;
        }

        if (element->child_count)
        {
            children = jalloc(sizeof*children * element->child_count);
            if (!children)
            {
                RMOD_ERROR("Failed jalloc(%zu)", sizeof*children * element->child_count);
                res = RMOD_RESULT_NOMEM;
                goto free_failed;
            }
            //  Convert child indices to use absolute values, rather than relative
            for (u32 k = 0; k < element->child_count; ++k)
            {
                assert(element->children[k] + i > 0);
                children[k] = (element_type_id)(element->children[k] + i);
            }
        }
        else
        {
            children = NULL;
        }

        this.node_list[i].type_id = type_index;
        this.node_list[i].parents = parents;
        this.node_list[i].parent_count = element->parent_count;
        this.node_list[i].children = children;
        this.node_list[i].child_count = element->child_count;
    }

    this.type_count = unique_types;
    this.type_list = jrealloc(type_array, unique_types * sizeof(*type_array));
    if (!this.type_list)
    {
        RMOD_WARN("Failed jrealloc(%p, %zu), but not critical since array was bigger than needed", type_array, unique_types * sizeof(*type_array));
        this.type_list = type_array;
    }

    *p_out = this;

    const u64 len_module_name = strlen(module_name);
    c8* const module_copy = jalloc(len_module_name + 1);
    if (!module_name)
    {
        RMOD_WARN("Failed jalloc(%zu), but not critical because it was for a name string", len_module_name + 1);
    }
    else
    {
        memcpy(module_copy, module_name, len_module_name + 1);
    }
    p_out->module_name = module_copy;

    c8* const name_copy = jalloc(target_chain->header.type_name.len + 1);
    if (!name_copy)
    {
        RMOD_WARN("Failed jalloc(%zu), but not critical because it was for a name string", target_chain->header.type_name.len + 1);
    }
    else
    {
        memcpy(name_copy, target_chain->header.type_name.begin, target_chain->header.type_name.len + 1);
        name_copy[target_chain->header.type_name.len] = 0;
    }
    p_out->graph_type = name_copy;


    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
free_failed:

    for (u32 k = 0; k < unique_types; ++k)
    {
        jfree(type_array[k].name);
    }
    for (u32 k = 0; k < i; ++k)
    {
        jfree(this.node_list[k].children);
        jfree(this.node_list[k].parents);
    }
    jfree(this.node_list);
failed:
    //  Reset linear allocator
    lin_jalloc_set_current(G_LIN_JALLOCATOR, base_lin_alloc);
    RMOD_LEAVE_FUNCTION;
    return res;
}

rmod_result rmod_destroy_graph(rmod_graph* graph)
{
    RMOD_ENTER_FUNCTION;

    for (u32 i = 0; i < graph->node_count; ++i)
    {
        jfree(graph->node_list[i].children);
        jfree(graph->node_list[i].parents);
    }
    jfree(graph->node_list);

    for (u32 i = 0; i < graph->type_count; ++i)
    {
        jfree(graph->type_list[i].name);
    }
    jfree(graph->type_list);

    jfree(graph->graph_type);
    jfree(graph->module_name);

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}
