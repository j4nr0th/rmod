//
// Created by jan on 10.6.2023.
//

#include <stdio.h>
#include "sstream.h"

string_stream string_stream_create(linear_jallocator* allocator)
{
    string_stream result;
    result.p_allocator = allocator;
    result.base = lin_jalloc_get_current(allocator);
    result.length = 0;
    return result;
}

size_t string_stream_add(string_stream* stream, const char* fmt, ...)
{
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);
    size_t written = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    char* new_base = lin_jrealloc(stream->p_allocator, stream->base, stream->length + written + 1);
    if (!new_base)
    {
        return -1;
    }
    stream->base = new_base;
    snprintf(new_base + stream->length, written + 1, fmt, args);
    va_end(args);
    stream->length += written;
    return written;
}

char* string_stream_contents(string_stream* stream)
{
    return stream->base;
}

size_t string_stream_add_str(string_stream* stream, const char* str)
{
    const size_t len = strlen(str);
    char* new_base = lin_jrealloc(stream->p_allocator, stream->base, stream->length + len + 1);
    if (!new_base)
    {
        return -1;
    }
    stream->base = new_base;
    memcpy(new_base + stream->length, str, len);
    stream->length += len;
    new_base[stream->length] = 0;
    return 0;
}

void string_stream_reset(string_stream* stream)
{
    lin_jalloc_set_current(stream->p_allocator, stream->base);
    stream->length = 0;
}

void string_stream_cleanup(string_stream* stream)
{
    string_stream_reset(stream);
    memset(stream, 0, sizeof(*stream));
}

size_t string_stream_len(string_stream* stream)
{
    return stream->length;
}
