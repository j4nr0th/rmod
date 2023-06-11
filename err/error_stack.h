//
// Created by jan on 2.4.2023.
//

#ifndef RMOD_ERROR_STACK_H
#define RMOD_ERROR_STACK_H
#include "error_codes.h"
#include "../common.h"
#include <errno.h>

typedef enum rmod_error_level_enum rmod_error_level;
enum rmod_error_level_enum
{
    RMOD_ERROR_LEVEL_NONE = 0,
    RMOD_ERROR_LEVEL_INFO = 1,
    RMOD_ERROR_LEVEL_WARN = 2,
    RMOD_ERROR_LEVEL_ERR = 3,
    RMOD_ERROR_LEVEL_CRIT = 4,
};

const char* rmod_error_level_str(rmod_error_level level);

void rmod_error_init_thread(char* thread_name, rmod_error_level level, u32 max_stack_trace, u32 max_errors);
void rmod_error_cleanup_thread(void);

u32 rmod_error_enter_function(const char* fn_name);
void rmod_error_leave_function(const char* fn_name, u32 level);

void rmod_error_push(rmod_error_level level, u32 line, const char* file, const char* function, const char* fmt, ...);
void rmod_error_report_critical(const char* fmt, ...);

typedef i32 (*rmod_error_report_fn)(u32 total_count, u32 index, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param);
typedef i32 (*rmod_error_hook_fn)(const char* thread_name, u32 stack_trace_count, const char*const* stack_trace, rmod_error_level level, u32 line, const char* file, const char* function, const char* message, void* param);

void rmod_error_process(rmod_error_report_fn function, void* param);
void rmod_error_peek(rmod_error_report_fn function, void* param);
void rmod_error_set_hook(rmod_error_hook_fn function, void* param);

#define RMOD_ERRNO_MESSAGE strerror(errno)
#define RMOD_ERROR(fmt, ...) rmod_error_push(RMOD_ERROR_LEVEL_ERR, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define RMOD_WARN(fmt, ...) rmod_error_push(RMOD_ERROR_LEVEL_WARN, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define RMOD_INFO(fmt, ...) rmod_error_push(RMOD_ERROR_LEVEL_INFO, __LINE__, __FILE__, __func__, fmt __VA_OPT__(,) __VA_ARGS__)
#define RMOD_ERROR_CRIT(fmt, ...) rmod_error_report_critical("JFW Error from \"%s\" at line %i: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#ifndef NDEBUG
#define RMOD_ENTER_FUNCTION const u32 _rmod_stacktrace_level = rmod_error_enter_function(__func__)
#define RMOD_LEAVE_FUNCTION rmod_error_leave_function(__func__, _rmod_stacktrace_level)
#else
#define RMOD_ENTER_FUNCTION (void)0
#define RMOD_LEAVE_FUNCTION (void)0
#endif
#endif //RMOD_ERROR_STACK_H
