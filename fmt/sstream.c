//
// Created by jan on 10.6.2023.
//

#include <stdio.h>
#include "sstream.h"
#include "../mem/jalloc.h"

struct string_stream_struct
{
    jallocator* p_allocator;
    linear_jallocator* p_lin_alloc;
    size_t length;
    size_t capacity;
    char* base;
    char* dbg;
};
size_t string_stream_create(linear_jallocator* lin_jallocator, jallocator* allocator, string_stream** p_stream)
{
    string_stream* this = jalloc(allocator, sizeof(*this));
    if (!this) return -1;
    memset(this, 0, sizeof(*this));
    this->p_allocator = allocator;
    this->base = jalloc(allocator, 4096);
    this->dbg = NULL;
    if (!this->base)
    {
        jfree(allocator, this);
        return -1;
    }
    this->p_lin_alloc = lin_jallocator;
    this->capacity = 4096;
    this->length = 0;
    *p_stream = this;
    return 0;
}

size_t string_stream_add(string_stream* stream, const char* fmt, ...)
{
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);
//    char* const formatted = lin_vasprintf(stream->p_lin_alloc, &written, fmt, args);
    size_t written = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (written == -1)
    {
        return -1;
    }
    if (stream->capacity <= written + stream->length)
    {

        uint_fast64_t page_count = (written + 1) >> 12;   //  Divide by 4096
        page_count += ((written + 1) & ((1 << 12) - 1)) != 0; //  If there is a remainder, then add an extra page
        const uint_fast64_t new_capacity = stream->capacity + (1 << 12) * page_count;
        char* const new_ptr = jrealloc(stream->p_allocator, stream->base, new_capacity);
        if (!new_ptr)
        {
            return -1;
        }
        stream->base = new_ptr;
        stream->capacity = new_capacity;
    }

    vsnprintf(stream->base + stream->length, stream->capacity - stream->length, fmt, args);
    va_end(args);
    stream->base[stream->length + written] = 0;
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
    if (stream->capacity <= len + stream->length)
    {
        uint_fast64_t page_count = (len + 1) >> 12;   //  Divide by 4096
        page_count += ((len + 1) & ((1 << 12) - 1)) != 0; //  If there is a remainder, then add an extra page
        const uint_fast64_t new_capacity = stream->capacity + (1 << 12) * page_count;
        char* const new_ptr = jrealloc(stream->p_allocator, stream->base, new_capacity);
        if (!new_ptr)
        {
            return -1;
        }
        stream->base = new_ptr;
        stream->capacity = new_capacity;
    }
    memcpy(stream->base + stream->length, str, len);
    stream->length += len;
    stream->base[stream->length] = 0;
    return 0;
}

void string_stream_reset(string_stream* stream)
{
    stream->length = 0;
}

void string_stream_cleanup(string_stream* stream)
{
    jallocator* const allocator = stream->p_allocator;
    jfree(allocator, stream->base);
    memset(stream, 0, sizeof(*stream));
    jfree(allocator, stream);
}

size_t string_stream_len(string_stream* stream)
{
    return stream->length;
}
