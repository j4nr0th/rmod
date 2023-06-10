//
// Created by jan on 2.4.2023.
//

#include "error_codes.h"
#include <stdio.h>
#include <string.h>

static const char* const ERROR_MESSAGES[RMOD_RESULT_COUNT] =
        {
                [RMOD_RESULT_SUCCESS] = "NO ERROR",
                [RMOD_RESULT_ERROR] = "GENERIC ERROR",
                [RMOD_RESULT_BAD_ERROR] = "INVALID ERROR CODE",
                [RMOD_RESULT_BAD_XML] = "FAILED PARSING XML FILE DUE TO FORMATING OR ANOTHER REASON",
                [RMOD_RESULT_BAD_CFG] = "FAILED PARSING CFG SPECIFICATION DUE TO FORMATTING OR ANOTHER REASON",
                [RMOD_RESULT_BAD_CLI] = "FAILED PARSING CLI OPTIONS GIVEN TO THE PROGRAM",
                [RMOD_RESULT_WAS_NULL] = "INPUT POINTER WAS NULL",
                [RMOD_RESULT_NOMEM] = "MEMORY ALLOCATION FAILED",
                [RMOD_RESULT_BAD_CHAIN_NAME] = "CHAIN NAME DID NOT REFFER TO A DEFINED CHAIN",
                [RMOD_RESULT_CYCLICAL_CHAIN] = "CHAIN CONTAINS A CYCLE",
                [RMOD_RESULT_CYCLICAL_CHAIN_DEPENDENCY] = "CHAIN DEPENDS ON CHAIN(S) WHICH DEPEND ON IT",
                [RMOD_RESULT_BAD_FILE_MAP] = "FILE COULD NOT BE MAPPED",
                [RMOD_RESULT_BAD_PATH] = "FILE PATH WAS INVALID",
                [RMOD_RESULT_BAD_VALUE] = "VALUE PROVIDED WAS INVALID",
                [RMOD_RESULT_BAD_THRD] = "THREAD COULD NOT BE CREATED",
                [RMOD_RESULT_STUPIDITY] = "ERROR OCCURRED DUE TO MY STUPIDITY",
        };

const char* rmod_result_str(rmod_result error_code)
{
    if (error_code >= 0 && error_code < RMOD_RESULT_COUNT) return ERROR_MESSAGES[error_code];
    return "Invalid error code";
}

rmod_result rmod_result_str_r(rmod_result error_code, size_t buffer_size, char* buffer)
{
    const char* message =  NULL;
    if (error_code >= 0 && error_code < RMOD_RESULT_COUNT)
    {
        message = ERROR_MESSAGES[error_code];
        const size_t len =strlen(message);
        if (len < buffer_size)
        {
            memcpy(buffer, message, len);
            buffer[len] = 0;
        }
        else
        {
            memcpy(buffer, message, buffer_size);
            buffer[buffer_size - 1] = 0;
        }
        return RMOD_RESULT_SUCCESS;
    }


    snprintf(buffer, buffer_size, "Unknown Error Code: %u", error_code);
    return RMOD_RESULT_BAD_ERROR;
}

