//
// Created by jan on 2.4.2023.
//

#ifndef RMOD_ERROR_CODES_H
#define RMOD_ERROR_CODES_H
#include <stdlib.h>

typedef enum rmod_result_enum rmod_result;

enum rmod_result_enum
{
    RMOD_RESULT_SUCCESS = 0,
    RMOD_RESULT_ERROR,
    RMOD_RESULT_BAD_ERROR,
    RMOD_RESULT_BAD_XML,
    RMOD_RESULT_WAS_NULL,
    RMOD_RESULT_NOMEM,
    RMOD_RESULT_BAD_CHAIN_NAME,
    RMOD_RESULT_CYCLICAL_CHAIN,
    RMOD_RESULT_CYCLICAL_CHAIN_DEPENDENCY,
    RMOD_RESULT_STUPIDITY,
    RMOD_RESULT_COUNT,  //  Has to be the last in the enum
};

const char* rmod_result_str(rmod_result error_code);

rmod_result rmod_result_str_r(rmod_result error_code, size_t buffer_size, char* restrict buffer);

#endif //RMOD_ERROR_CODES_H
