//
// Created by jan on 27.5.2023.
//

#ifndef RMOD_COMMON_H
#define RMOD_COMMON_H
//  Standard library includes
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <wchar.h>
#include <uchar.h>

#include <stdbool.h>

//  Typedefs used for the project
typedef uint_fast64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int_fast64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef double_t f64;
typedef float_t f32;

typedef void u0, i0;
#ifdef _WIN32
#include <Windows.h>
#endif
typedef unsigned char c8;
typedef uint_least16_t c16;
typedef uint_least32_t c32;


//  My custom memory allocation functions
#include "mem/jalloc.h"
#include "mem/lin_jalloc.h"

extern jallocator* G_JALLOCATOR;
extern linear_jallocator* G_LIN_JALLOCATOR;

#ifndef NDEBUG
//  When in debug mode, jallocator is not used, so that malloc can be used for debugging instead
#define jalloc(size) malloc(size)
#define jfree(ptr) free(ptr)
#define jrealloc(ptr, new_size) realloc(ptr, new_size)
#else
#define jalloc(size) jalloc(G_JALLOCATOR, (size))
#define jfree(ptr) jfree(G_JALLOCATOR, (ptr))
#define jrealloc(ptr, new_size) jrealloc(G_JALLOCATOR, (ptr), (new_size))
#endif

#endif //RMOD_COMMON_H
