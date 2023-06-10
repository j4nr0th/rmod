//
// Created by jan on 10.6.2023.
//

#ifndef JSTR_SSTREAM_H
#define JSTR_SSTREAM_H
#include "sformatted.h"

typedef struct string_stream_struct string_stream;
struct string_stream_struct
{
    linear_jallocator* p_allocator;
    char* base;
    size_t length;
};

string_stream string_stream_create(linear_jallocator* allocator);

size_t string_stream_add(string_stream* stream, const char* fmt, ...);

char* string_stream_contents(string_stream* stream);

size_t string_stream_add_str(string_stream* stream, const char* str);

size_t string_stream_len(string_stream* stream);

void string_stream_reset(string_stream* stream);

void string_stream_cleanup(string_stream* stream);



#endif //JSTR_SSTREAM_H
