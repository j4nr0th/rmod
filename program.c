//
// Created by jan on 31.5.2023.
//

#include <unistd.h>
#include <libgen.h>
#include "program.h"

rmod_result rmod_program_create(const char* file_name, rmod_program* p_program)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;

    char name_path_buffer[PATH_MAX];
    if (!realpath(file_name, name_path_buffer))
    {
        RMOD_ERROR("Could not get the real path of file \"%s\", reason: %s", file_name, RMOD_ERRNO_MESSAGE);
        res = RMOD_RESULT_BAD_PATH;
        goto end;
    }
    char prev_wd[PATH_MAX], new_wd[PATH_MAX];
    if (!getcwd(prev_wd, PATH_MAX))
    {
        RMOD_ERROR("Failed getting the current working directory, reason: %s", RMOD_ERRNO_MESSAGE);
        res = RMOD_RESULT_BAD_PATH;
        goto end;
    }
    //  Literally same size
    strncpy(new_wd, name_path_buffer, PATH_MAX);
    if (chdir(dirname(new_wd)) < 0)
    {
        RMOD_ERROR("Could not change working directory to \"%s\", reason: %s", new_wd, RMOD_ERRNO_MESSAGE);
        res = RMOD_RESULT_BAD_PATH;
        goto end;
    }

    u32 file_count = 1;
    u32 file_capacity = 16;
    rmod_memory_file* mem_file_array = jalloc(sizeof(*mem_file_array) * file_capacity);
    if (!mem_file_array)
    {
        chdir(prev_wd);
        RMOD_ERROR("Failed jalloc(%zu)", sizeof(*mem_file_array) * file_capacity);
        res = RMOD_RESULT_NOMEM;
        goto end;
    }
    res = map_file_to_memory(name_path_buffer, mem_file_array + 0);
    if (res != RMOD_RESULT_SUCCESS)
    {
        jfree(mem_file_array);
        chdir(prev_wd);
        RMOD_ERROR("Failed mapping base file \"%s\" to memory, reason: %s", name_path_buffer, RMOD_ERRNO_MESSAGE);
        goto end;
    }

    xml_element file_root;
    res = rmod_parse_xml(mem_file_array + 0, &file_root);
    if (res != RMOD_RESULT_SUCCESS)
    {
        unmap_file(mem_file_array + 0);
        jfree(mem_file_array);
        chdir(prev_wd);
        RMOD_ERROR("Failed mapping base file \"%s\" to memory, reason: %s", name_path_buffer, RMOD_ERRNO_MESSAGE);
        goto end;
    }

    u32 n_types = 0;
    rmod_element_type* p_types = NULL;
    res = rmod_convert_xml(&file_root, &file_count, &file_capacity, &mem_file_array, &n_types, &p_types);
    chdir(prev_wd);
    if (res != RMOD_RESULT_SUCCESS)
    {
        for (u32 i = 0; i < file_count; ++i)
        {
            unmap_file(mem_file_array + i);
        }
        jfree(mem_file_array);
        chdir(prev_wd);
        RMOD_ERROR("Failed conversion of base xml to program");
        goto end;
    }

    {
        rmod_element_type* const new_ptr = jrealloc(p_types, n_types * sizeof(*p_types));
        if (!new_ptr)
        {
            RMOD_WARN("Failed jrealloc(%p, %zu), but it's not critical because buffer was already large enough", p_types, n_types * sizeof(*p_types));
        }
        else
        {
            p_types = new_ptr;
        }
    }
    {
        rmod_memory_file * const new_ptr = jrealloc(mem_file_array, file_count * sizeof(*mem_file_array));
        if (!new_ptr)
        {
            RMOD_WARN("Failed jrealloc(%p, %zu), but it's not critical because buffer was already large enough", mem_file_array, file_count * sizeof(*p_types));
        }
        else
        {
            mem_file_array = new_ptr;
        }
    }

    *p_program = (rmod_program){
        .p_types = p_types,
        .n_types = n_types,
        .mem_files = mem_file_array,
        .n_files = file_count,
        .xml_root = file_root,
    };

end:
    RMOD_LEAVE_FUNCTION;
    return res;
}

rmod_result rmod_program_delete(rmod_program* program)
{
    RMOD_ENTER_FUNCTION;
    rmod_release_xml(&program->xml_root);
    rmod_destroy_types(program->n_types, program->p_types);
    for (u32 i = 0; i < program->n_files; ++i)
    {
        unmap_file(program->mem_files + i);
    }
    memset(program->mem_files, 0, sizeof(*program->mem_files) * program->n_files);
    jfree(program->mem_files);
    memset(program, 0, sizeof(*program));

    RMOD_LEAVE_FUNCTION;
    return RMOD_RESULT_SUCCESS;
}

rmod_result
rmod_compile(const rmod_program* program, rmod_graph* graph, const char* chain_name, const char* module_name)
{
    RMOD_ENTER_FUNCTION;

    rmod_result res = rmod_compile_graph(program->n_types, program->p_types, chain_name, module_name, graph);

    RMOD_LEAVE_FUNCTION;
    return res;
}
