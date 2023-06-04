//
// Created by jan on 27.4.2023.
//

#ifndef RMOD_JALLOC_H
#define RMOD_JALLOC_H
#include <stdint.h>

typedef struct jallocator_struct jallocator;

jallocator* jallocator_create(uint_fast64_t pool_size, uint_fast64_t malloc_limit, uint_fast64_t initial_pool_count);

int jallocator_verify(jallocator* allocator, int_fast32_t* i_pool, int_fast32_t* i_block);

uint_fast64_t jallocator_destroy(jallocator* allocator);

void* jalloc(jallocator* allocator, uint_fast64_t size);

void* jrealloc(jallocator* allocator, void* ptr, uint_fast64_t new_size);

void jfree(jallocator* allocator, void* ptr);

uint_fast32_t jallocator_count_used_blocks(jallocator* allocator, uint_fast32_t size_out_buffer, uint_fast32_t* out_buffer);

#endif //RMOD_JALLOC_H
