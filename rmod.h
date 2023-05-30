//
// Created by jan on 27.5.2023.
//

#ifndef RMOD_RMOD_H
#define RMOD_RMOD_H

#include "common.h"
#include "err/error_stack.h"

void* map_file_to_memory(const char* filename, u64* p_out_size);

void unmap_file(void* ptr, u64 size);

#endif //RMOD_RMOD_H
