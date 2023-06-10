//
// Created by jan on 13.5.2023.
//

#ifndef JSTR_CFORMATTED_H
#define JSTR_CFORMATTED_H
#include "../mem/lin_jalloc.h"
#include <string.h>
#include <stdarg.h>

size_t cprintf(linear_jallocator* support_allocator, const char* fmt, ...);

size_t vacprintf(linear_jallocator* support_allocator, const char* fmt, va_list args);

#endif //JSTR_CFORMATTED_H
