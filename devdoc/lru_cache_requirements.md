# `lru_cache` requirements

## Overview

A cache module that deletes the least-recently-used (lru) items. The design for this is [here]()

## Exposed API

```c
typedef struct LRU_CACHE_TAG* LRU_CACHE_HANDLE;

#define LRU_CACHE_PUT_RESULT_VALUES \
    LRU_CACHE_PUT_OK, \
    LRU_CACHE_PUT_ERROR, \
    LRU_CACHE_PUT_VALUE_TOO_BIG

MU_DEFINE_ENUM(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);

#define LRU_CACHE_EVICT_RESULT_VALUES \
    LRU_CACHE_EVICT_OK, \
    LRU_CACHE_EVICT_ERROR

MU_DEFINE_ENUM(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT_VALUES);

typedef void(*LRU_CACHE_EVICT_CALLBACK_FUNC)(void* context, LRU_CACHE_EVICT_RESULT cache_evict_status);

MOCKABLE_FUNCTION(, LRU_CACHE_HANDLE, lru_cache_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, volatile_atomic int64_t*, start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context, int64_t, capacity);

MOCKABLE_FUNCTION(, void, lru_cache_destroy, LRU_CACHE_HANDLE, lru_cache);

MOCKABLE_FUNCTION(, LRU_CACHE_PUT_RESULT, lru_cache_put, LRU_CACHE_HANDLE, lru_handle, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t, size, int64_t, seq_no, LRU_CACHE_EVICT_CALLBACK_FUNC, evict_callback, void*, evict_context);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, lru_cache_get, LRU_CACHE_HANDLE, lru_cache, void*, key);
```

### clds_hash_table_create

```c
MOCKABLE_FUNCTION(, LRU_CACHE_HANDLE, lru_cache_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, volatile_atomic int64_t*, start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context, int64_t, capacity);
```

Creates `LRU_CACHE_HANDLE` which holds `clds_hash_table`, `doublylinkedlist` and `SRW_LOCK_HANDLE`.

**SRS_LRU_CACHE_13_001: [** If `compute_hash` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_002: [** If `key_compare_func` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_003: [** If `initial_bucket_size` is `0`, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_004: [** If `clds_hazard_pointers` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_005: [** `start_sequence_number` shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. **]**

**SRS_LRU_CACHE_13_006: [** `start_sequence_number` shall be allowed to be NULL, in which case no sequence number computations shall be performed. **]**

**SRS_LRU_CACHE_13_007: [** `skipped_seq_no_cb` shall be allowed to be NULL. **]**

**SRS_LRU_CACHE_13_008: [** `skipped_seq_no_cb_context` shall be allowed to be NULL. **]**

**SRS_LRU_CACHE_13_009: [** If `start_sequence_number` is NULL, then `skipped_seq_no_cb` must also be NULL, otherwise `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_010: [** If `capacity` is less than or equals to `0`, then `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_011: [** `lru_cache_create` shall allocate memory for `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_012: [** `lru_cache_create` shall assign `start_sequence_number` to `seq_no` if not `NULL`, otherwise `seq_no` is defaulted to `0`. **]**

**SRS_LRU_CACHE_13_013: [** `lru_cache_create` shall assign `clds_hazard_pointers` to `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_014: [** `lru_cache_create` shall allocate `CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE` by calling `clds_hazard_pointers_thread_helper_create`. **]**

**SRS_LRU_CACHE_13_015: [** `lru_cache_create` shall allocate `clds_hash_table` by calling `clds_hash_table_create`. **]**

**SRS_LRU_CACHE_13_016: [** `lru_cache_create` shall allocate `SRW_LOCK_HANDLE` by calling `srw_lock_create`. **]**

**SRS_LRU_CACHE_13_017: [** `lru_cache_create` shall initialize `head` by calling `DList_InitializeListHead`. **]**

**SRS_LRU_CACHE_13_018: [** `lru_cache_create` shall assign default value of `0` to `current_size` and `capacity`. **]**

**SRS_LRU_CACHE_13_019: [** On success, `lru_cache_create` shall return `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_020: [** If there are any failures then `lru_cache_create` shall fail and return `NULL`. **]**


### lru_cache_destroy

```c
void lru_cache_destroy(LRU_CACHE_HANDLE lru_cache);
```

Frees up `LRU_CACHE_HANDLE`.

**SRS_LRU_CACHE_13_021: [** If `lru_cache` is `NULL`, then `lru_cache_destroy` shall fail. **]**

**SRS_LRU_CACHE_13_022: [** `lru_cache_destroy` shall free all resources associated with the `LRU_CACHE_HANDLE`. **]**


### lru_cache_put

```c
MOCKABLE_FUNCTION(, LRU_CACHE_PUT_RESULT, lru_cache_put, LRU_CACHE_HANDLE, lru_handle, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t, size, int64_t, seq_no, LRU_CACHE_EVICT_CALLBACK_FUNC, evict_callback, void*, evict_context);
```

