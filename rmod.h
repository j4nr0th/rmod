//
// Created by jan on 27.5.2023.
//

#ifndef RMOD_RMOD_H
#define RMOD_RMOD_H
#include <limits.h>
#include "common.h"
#include "err/error_stack.h"

typedef struct rmod_memory_file_struct rmod_memory_file;

struct rmod_memory_file_struct
{
    void* ptr;
    u64 file_size;
#ifndef _WIN32
    char name[PATH_MAX];
#else
#error NOT WRITTEN
#endif
};

rmod_result map_file_to_memory(const char* filename, rmod_memory_file* p_file_out);

void unmap_file(rmod_memory_file* p_file_out);

bool map_file_is_named(const rmod_memory_file* f1, const char* filename);

#endif //RMOD_RMOD_H
