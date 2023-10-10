// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "umock_c/umock_c_prod.h"
#include "clds_hazard_pointers.h"
#include "clds_hash_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LRU_CACHE_TAG* LRU_CACHE_HANDLE;

#define LRU_CACHE_PUT_RESULT_VALUES \
    LRU_CACHE_PUT_OK, \
    LRU_CACHE_PUT_ERROR, \
    LRU_CACHE_PUT_VALUE_INVALID_SIZE

MU_DEFINE_ENUM(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);

#define LRU_CACHE_EVICT_RESULT_VALUES \
    LRU_CACHE_EVICT_OK, \
    LRU_CACHE_EVICT_ERROR

MU_DEFINE_ENUM(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT_VALUES);

typedef void(*LRU_CACHE_EVICT_CALLBACK_FUNC)(void* context, LRU_CACHE_EVICT_RESULT cache_evict_status);

MOCKABLE_FUNCTION(, LRU_CACHE_HANDLE, lru_cache_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, volatile_atomic int64_t*, start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context, uint64_t, capacity);

MOCKABLE_FUNCTION(, void, lru_cache_destroy, LRU_CACHE_HANDLE, lru_cache);

MOCKABLE_FUNCTION(, LRU_CACHE_PUT_RESULT, lru_cache_put, LRU_CACHE_HANDLE, lru_handle, void*, key, CLDS_HASH_TABLE_ITEM*, value, uint64_t, size, int64_t*, seq_no, LRU_CACHE_EVICT_CALLBACK_FUNC, evict_callback, void*, evict_context);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, lru_cache_get, LRU_CACHE_HANDLE, lru_cache, void*, key);

#ifdef __cplusplus
}
#endif

#endif /* LRU_CACHE_H */