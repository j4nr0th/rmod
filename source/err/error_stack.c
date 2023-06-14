//
// Created by jan on 2.4.2023.
//

#include "error_stack.h"
#include "../common/common.h"
#include <stdio.h>
#include <stdarg.h>

struct rmod_error_message
{
    rmod_error_level level;
    const char* file;
    const char* function;
    u32 line;
    char message[];
};

typedef struct rmod_error_state_struct rmod_error_state;
struct rmod_error_state_struct
{
    i32 init;

    char* thrd_name;
    size_t len_name;

    u32 error_count;
    u32 error_capacity;
    struct rmod_error_message** errors;

    u32 stacktrace_count;
    u32 stacktrace_capacity;
    const char** stack_traces;

    rmod_error_level level;

    rmod_error_hook_fn hook;
    void* hook_param;
};

_Thread_local rmod_error_state RMOD_THREAD_ERROR_STATE;

void rmod_error_init_thread(char* thread_name, rmod_error_level level, u32 max_stack_trace, u32 max_errors)
{
    assert(RMOD_THREAD_ERROR_STATE.init == 0);
    RMOD_THREAD_ERROR_STATE.thrd_name = thread_name ? strdup(thread_name) : NULL;
    assert(RMOD_THREAD_ERROR_STATE.thrd_name != NULL);
    RMOD_THREAD_ERROR_STATE.len_name = thread_name ? strlen(thread_name) : 0;
    RMOD_THREAD_ERROR_STATE.level = level;
    RMOD_THREAD_ERROR_STATE.errors = calloc(max_errors, sizeof(*RMOD_THREAD_ERROR_STATE.errors));
    assert(RMOD_THREAD_ERROR_STATE.errors != NULL);
    RMOD_THREAD_ERROR_STATE.error_capacity = max_errors;

    RMOD_THREAD_ERROR_STATE.stack_traces = calloc(max_stack_trace, sizeof(char*));
    assert(RMOD_THREAD_ERROR_STATE.stack_traces != NULL);
    RMOD_THREAD_ERROR_STATE.stacktrace_capacity = max_stack_trace;
}

void rmod_error_cleanup_thread(void)
{
    assert(RMOD_THREAD_ERROR_STATE.stacktrace_count == 0);
    free(RMOD_THREAD_ERROR_STATE.thrd_name);
    for (u32 i = 0; i < RMOD_THREAD_ERROR_STATE.error_count; ++i)
    {
        free(RMOD_THREAD_ERROR_STATE.errors[i]);
    }
    free(RMOD_THREAD_ERROR_STATE.errors);
    free(RMOD_THREAD_ERROR_STATE.stack_traces);
    memset(&RMOD_THREAD_ERROR_STATE, 0, sizeof(RMOD_THREAD_ERROR_STATE));
}

u32 rmod_error_enter_function(const char* fn_name)
{
    u32 id = RMOD_THREAD_ERROR_STATE.stacktrace_count;
    if (RMOD_THREAD_ERROR_STATE.stacktrace_count < RMOD_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        RMOD_THREAD_ERROR_STATE.stack_traces[RMOD_THREAD_ERROR_STATE.stacktrace_count++] = fn_name;
    }
    return id;
}

void rmod_error_leave_function(const char* fn_name, u32 level)
{
    if (level < RMOD_THREAD_ERROR_STATE.stacktrace_capacity)
    {
        assert(RMOD_THREAD_ERROR_STATE.stacktrace_count == level + 1);
        assert(RMOD_THREAD_ERROR_STATE.stack_traces[level] == fn_name);
        RMOD_THREAD_ERROR_STATE.stacktrace_count = level;
    }
    else
    {
        assert(level == RMOD_THREAD_ERROR_STATE.stacktrace_capacity);
        assert(RMOD_THREAD_ERROR_STATE.stacktrace_count == RMOD_THREAD_ERROR_STATE.stacktrace_capacity);
    }
}

void rmod_error_push(rmod_error_level level, u32 line, const char* file, const char* function, const char* fmt, ...)
{
    if (level < RMOD_THREAD_ERROR_STATE.level || RMOD_THREAD_ERROR_STATE.error_count == RMOD_THREAD_ERROR_STATE.error_capacity) return;
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);
    const size_t error_length = RMOD_THREAD_ERROR_STATE.len_name + vsnprintf(NULL, 0, fmt, args1) + 4;
    va_end(args1);
    struct rmod_error_message* message = malloc(sizeof(*message) + error_length);
    assert(message);
    size_t used = vsnprintf(message->message, error_length, fmt, args2);
    va_end(args2);
    snprintf(message->message + used, error_length - used, " (%s)", RMOD_THREAD_ERROR_STATE.thrd_name);
    i32 put_in_stack = 1;
    if (RMOD_THREAD_ERROR_STATE.hook)
    {
        put_in_stack = RMOD_THREAD_ERROR_STATE.hook(RMOD_THREAD_ERROR_STATE.thrd_name, RMOD_THREAD_ERROR_STATE.stacktrace_count, RMOD_THREAD_ERROR_STATE.stack_traces, level, line, file, function, message->message, RMOD_THREAD_ERROR_STATE.hook_param);
    }

    if (put_in_stack)
    {
        message->file = file;
        message->level = level;
        message->function = function;
        message->line = line;
        RMOD_THREAD_ERROR_STATE.errors[RMOD_THREAD_ERROR_STATE.error_count++] = message;
    }
    else
    {
        free(message);
    }
}

void rmod_error_report_critical(const char* fmt, ...)
{
    fprintf(stderr, "Critical error:\n");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\t(%s : %s", RMOD_THREAD_ERROR_STATE.thrd_name, RMOD_THREAD_ERROR_STATE.stack_traces[0]);
    for (u32 i = 1; i < RMOD_THREAD_ERROR_STATE.stacktrace_count; ++i)
    {
        fprintf(stderr, " -> %s", RMOD_THREAD_ERROR_STATE.stack_traces[i]);
    }
    fprintf(stderr, ")\nTerminating program\n");
    exit(EXIT_FAILURE);
}

void rmod_error_process(rmod_error_report_fn function, void* param)
{
    assert(function);
    u32 i;
    for (i = 0; i < RMOD_THREAD_ERROR_STATE.error_count; ++i)
    {
        struct rmod_error_message* msg = RMOD_THREAD_ERROR_STATE.errors[RMOD_THREAD_ERROR_STATE.error_count - 1 - i];
        const i32 ret = function(RMOD_THREAD_ERROR_STATE.error_count, i, msg->level, msg->line, msg->file, msg->function, msg->message, param);
        if (ret < 0) break;
    }
    for (u32 j = 0; j < i; ++j)
    {
        struct rmod_error_message* msg = RMOD_THREAD_ERROR_STATE.errors[RMOD_THREAD_ERROR_STATE.error_count - 1 - j];
        RMOD_THREAD_ERROR_STATE.errors[RMOD_THREAD_ERROR_STATE.error_count - 1 - j] = NULL;
        free(msg);
    }
    RMOD_THREAD_ERROR_STATE.error_count = 0;
}

void rmod_error_peek(rmod_error_report_fn function, void* param)
{
    assert(function);
    u32 i;
    for (i = 0; i < RMOD_THREAD_ERROR_STATE.error_count; ++i)
    {
        struct rmod_error_message* msg = RMOD_THREAD_ERROR_STATE.errors[RMOD_THREAD_ERROR_STATE.error_count - 1 - i];
        const i32 ret = function(RMOD_THREAD_ERROR_STATE.error_count, RMOD_THREAD_ERROR_STATE.error_count - 1 - i, msg->level, msg->line, msg->file, msg->function, msg->message, param);
        if (ret < 0) break;
    }
}

void rmod_error_set_hook(rmod_error_hook_fn function, void* param)
{
    RMOD_THREAD_ERROR_STATE.hook = function;
    RMOD_THREAD_ERROR_STATE.hook_param = param;
}

const char* rmod_error_level_str(rmod_error_level level)
{
    static const char* const NAMES[] =
            {
                    [RMOD_ERROR_LEVEL_NONE] = "RMOD_ERROR_LEVEL_NONE" ,
                    [RMOD_ERROR_LEVEL_INFO] = "RMOD_ERROR_LEVEL_INFO" ,
                    [RMOD_ERROR_LEVEL_WARN] = "RMOD_ERROR_LEVEL_WARN" ,
                    [RMOD_ERROR_LEVEL_ERR] = "RMOD_ERROR_LEVEL_ERR"  ,
                    [RMOD_ERROR_LEVEL_CRIT] = "RMOD_ERROR_LEVEL_CRIT" ,
            };
    if (level >= RMOD_ERROR_LEVEL_NONE && level <= RMOD_ERROR_LEVEL_CRIT)
    {
        return NAMES[level];
    }
    return NULL;
}
