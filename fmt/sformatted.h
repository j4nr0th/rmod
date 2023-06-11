//
// Created by jan on 13.5.2023.
//

#ifndef JSTR_SFORMATTED_H
#define JSTR_SFORMATTED_H
#include "../mem/lin_jalloc.h"
#include <string.h>
#include <stdarg.h>


char* lin_sprintf(linear_jallocator* allocator, size_t* p_size, const char* fmt, ...);

char* lin_aprintf(linear_jallocator* allocator, char* previous, size_t* p_size, const char* fmt, ...);

char* lin_vasprintf(linear_jallocator* allocator, size_t* p_size, const char* fmt, va_list args);

char* lin_avaprintf(linear_jallocator* allocator, char* previous, size_t* p_size, const char* fmt, va_list args);

void lin_eprintf(linear_jallocator* allocator, const char* fmt, ...);

#endif //JSTR_SFORMATTED_H
