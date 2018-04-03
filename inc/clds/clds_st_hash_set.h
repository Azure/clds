// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_ST_HASH_SET_H
#define CLDS_ST_HASH_SET_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct CLDS_ST_HASH_SET_TAG* CLDS_ST_HASH_SET_HANDLE;
typedef uint64_t (*CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC)(void* key);

MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size);
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
MOCKABLE_FUNCTION(, int, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
MOCKABLE_FUNCTION(, int, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key, bool*, key_found);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_ST_HASH_SET_H */
