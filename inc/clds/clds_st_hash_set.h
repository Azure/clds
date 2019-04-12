// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_ST_HASH_SET_H
#define CLDS_ST_HASH_SET_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

typedef struct CLDS_ST_HASH_SET_TAG* CLDS_ST_HASH_SET_HANDLE;
typedef uint64_t (*CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC)(void* key);
typedef int(*CLDS_ST_HASH_SET_KEY_COMPARE_FUNC)(void* key_1, void* key_2);

#define CLDS_ST_HASH_SET_INSERT_RESULT_VALUES \
    CLDS_ST_HASH_SET_INSERT_OK, \
    CLDS_ST_HASH_SET_INSERT_ERROR, \
    CLDS_ST_HASH_SET_INSERT_KEY_ALREADY_EXISTS

MU_DEFINE_ENUM(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT_VALUES);

#define CLDS_ST_HASH_SET_FIND_RESULT_VALUES \
    CLDS_ST_HASH_SET_FIND_OK, \
    CLDS_ST_HASH_SET_FIND_ERROR, \
    CLDS_ST_HASH_SET_FIND_NOT_FOUND

MU_DEFINE_ENUM(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT_VALUES);

MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, key_compare_func, size_t, bucket_size);
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_INSERT_RESULT, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_FIND_RESULT, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_ST_HASH_SET_H */
