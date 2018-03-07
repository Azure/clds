// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_HASH_TABLE_H
#define CLDS_HASH_TABLE_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, void*, key, void*, value);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HASH_TABLE_H */
