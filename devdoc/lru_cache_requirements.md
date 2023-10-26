# `lru_cache` requirements

## Overview

A cache module that deletes the least-recently-used (lru) items. The design for this is [here](lru_cache_design.md)

## Exposed API

```c
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


typedef void(*LRU_CACHE_EVICT_CALLBACK_FUNC)(void* context, LRU_CACHE_EVICT_RESULT cache_evict_status, void* evicted_value);

MOCKABLE_FUNCTION(, LRU_CACHE_HANDLE, lru_cache_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, uint64_t, capacity);

MOCKABLE_FUNCTION(, void, lru_cache_destroy, LRU_CACHE_HANDLE, lru_cache);

MOCKABLE_FUNCTION(, LRU_CACHE_PUT_RESULT, lru_cache_put, LRU_CACHE_HANDLE, lru_handle, void*, key, void*, value, uint64_t, size, LRU_CACHE_EVICT_CALLBACK_FUNC, evict_callback, void*, evict_context);

MOCKABLE_FUNCTION(, void*, lru_cache_get, LRU_CACHE_HANDLE, lru_cache, void*, key);
```

### clds_hash_table_create

```c
MOCKABLE_FUNCTION(, LRU_CACHE_HANDLE, lru_cache_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, uint64_t, capacity);
```

Creates `LRU_CACHE_HANDLE` which holds `clds_hash_table`, `doublylinkedlist` and `SRW_LOCK_HANDLE`.

**SRS_LRU_CACHE_13_001: [** If `compute_hash` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_002: [** If `key_compare_func` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_003: [** If `initial_bucket_size` is `0`, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_004: [** If `clds_hazard_pointers` is NULL, `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_010: [** If `capacity` is `0`, then `lru_cache_create` shall fail and return NULL. **]**

**SRS_LRU_CACHE_13_011: [** `lru_cache_create` shall allocate memory for `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_013: [** `lru_cache_create` shall assign `clds_hazard_pointers` to `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_014: [** `lru_cache_create` shall allocate `CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE` by calling `clds_hazard_pointers_thread_helper_create`. **]**

**SRS_LRU_CACHE_13_015: [** `lru_cache_create` shall allocate `clds_hash_table` by calling `clds_hash_table_create`. **]**

**SRS_LRU_CACHE_13_016: [** `lru_cache_create` shall allocate `SRW_LOCK_LL` by calling `srw_lock_ll_init`. **]**

**SRS_LRU_CACHE_13_017: [** `lru_cache_create` shall initialize `head` by calling `DList_InitializeListHead`. **]**

**SRS_LRU_CACHE_13_018: [** `lru_cache_create` shall assign default value of `0` to `current_size` and the capacity to `capacity`. **]**

**SRS_LRU_CACHE_13_019: [** On success, `lru_cache_create` shall return `LRU_CACHE_HANDLE`. **]**

**SRS_LRU_CACHE_13_020: [** If there are any failures then `lru_cache_create` shall fail and return `NULL`. **]**


### lru_cache_destroy

```c
MOCKABLE_FUNCTION(, void, lru_cache_destroy, LRU_CACHE_HANDLE, lru_cache);
```

Frees up `LRU_CACHE_HANDLE`.

**SRS_LRU_CACHE_13_021: [** If `lru_cache` is `NULL`, then `lru_cache_destroy` shall return. **]**

**SRS_LRU_CACHE_13_022: [** `lru_cache_destroy` shall free all resources associated with the `LRU_CACHE_HANDLE`. **]**


### lru_cache_put

```c
MOCKABLE_FUNCTION(, LRU_CACHE_PUT_RESULT, lru_cache_put, LRU_CACHE_HANDLE, lru_handle, void*, key, void*, value, uint64_t, size, LRU_CACHE_EVICT_CALLBACK_FUNC, evict_callback, void*, evict_context);
```

The `lru_cache_put` function is utilized for inserting or updating an item in the Least Recently Used (LRU) cache. If the item already exists in the cache, the `current_size` is updated first, and then the value is reinserted into the cache to maintain the LRU order and triggers eviction if needed. In case the item is not found, it adds the item to the cache  and performs eviction if necessary. The eviction process involves updating the cache's current size, removing the least recently used item, and invoking an eviction callback. All the latest items are inserted at the tail of the `doubly_linked_list`. During eviction, the node next to the head (i.e., the least recently used item) is selected and removed from the `clds_hash_table`. It's important to note that the `current_size` may temporarily increase during this process, but eviction ensures the `current_size` is normalized.

Note: The `size` of the value needs to be precalculated in terms of the `capacity` mentioned at the creation of the cache.

**SRS_LRU_CACHE_13_023: [** If `lru_handle` is `NULL`, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_ERROR`. **]**

**SRS_LRU_CACHE_13_024: [** If `key` is `NULL`, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_ERROR`. **]**

**SRS_LRU_CACHE_13_025: [** If `value` is `NULL`, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_ERROR`. **]**

**SRS_LRU_CACHE_13_026: [** If `size` is `0`, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_ERROR`. **]**

**SRS_LRU_CACHE_13_075: [** If `evict_callback` is `NULL`, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_ERROR`. **]**

**SRS_LRU_CACHE_13_076: [** `context` may be `NULL`. **]**

**SRS_LRU_CACHE_13_027: [** If `size` is greater than `capacity` of lru cache, then `lru_cache_put` shall fail and return `LRU_CACHE_PUT_VALUE_INVALID_SIZE`. **]**

**SRS_LRU_CACHE_13_028: [** `lru_cache_put` shall get `CLDS_HAZARD_POINTERS_THREAD_HANDLE` by calling `clds_hazard_pointers_thread_helper_get_thread`. **]**

**SRS_LRU_CACHE_13_029: [** `lru_cache_put` shall check hash table for any existence of the value by calling `clds_hash_table_find` on the `key`. **]**

**SRS_LRU_CACHE_13_030: [**  If the `key` is found: **]**

- **SRS_LRU_CACHE_13_064: [** `lru_cache_put` shall create LRU Node item to be updated in the hash table. **]**

- **SRS_LRU_CACHE_13_065: [** `lru_cache_put` shall update the LRU Node item in the hash table by calling `clds_hash_table_set_value`. **]**

- **SRS_LRU_CACHE_13_033: [** `lru_cache_put` shall acquire the lock in exclusive mode. **]**

- **SRS_LRU_CACHE_13_077: [** If LRU Node state is `LRU_NODE_STATE_READY` only then the old Node is removed from list by calling `DList_RemoveEntryList`. **]**

- **SRS_LRU_CACHE_13_066: [** `lru_cache_put` shall append the updated node to the tail to maintain the order. **]**

- **SRS_LRU_CACHE_13_036: [** `lru_cache_put` shall release the lock in exclusive mode. **]**

- **SRS_LRU_CACHE_13_070: [** `lru_cache_put` shall update the `current_size` with the new `size` and removes the old value size. **]**

- **SRS_LRU_CACHE_13_067: [** `lru_cache_put` shall free the old value. **]**

- **SRS_LRU_CACHE_13_068: [** `lru_cache_put` shall return with `LRU_CACHE_PUT_OK`. **]**

**SRS_LRU_CACHE_13_071: [** Otherwise, if the `key` is not found: **]**

- **SRS_LRU_CACHE_13_044: [** `lru_cache_put` shall create LRU Node item to be inserted in the hash table. **]**

- **SRS_LRU_CACHE_13_045: [** `lru_cache_put` shall insert the LRU Node item in the hash table by calling `clds_hash_table_insert`. **]**

- **SRS_LRU_CACHE_13_046: [** `lru_cache_put` shall acquire the lock in exclusive mode. **]**

- **SRS_LRU_CACHE_13_047: [** `lru_cache_put` shall append the node to the tail. **]**

- **SRS_LRU_CACHE_13_048: [** `lru_cache_put` shall release the lock in exclusive mode. **]**

- **SRS_LRU_CACHE_13_062: [** `lru_cache_put` shall add the item `size` to the `current_size`. **]**

**SRS_LRU_CACHE_13_037: [** While the `current_size` of the cache exceeds `capacity`: **]**

- **SRS_LRU_CACHE_13_040: [** `lru_cache_put` shall acquire the lock in exclusive. **]**

- **SRS_LRU_CACHE_13_038: [** `lru_cache_put` shall get the least used node which is `Flink` of head node. **]**


- **SRS_LRU_CACHE_13_072: [** `lru_cache_put` shall decrement the least used node size from `current_size`. **]**

- **SRS_LRU_CACHE_13_039: [** The least used node is removed from `clds_hash_table` by calling `clds_hash_table_remove`. **]**

- **SRS_LRU_CACHE_13_078: [** If `clds_hash_table_remove` returns `CLDS_HASH_TABLE_REMOVE_NOT_FOUND`, then `lru_cache_put` shall retry eviction. **]**

- **SRS_LRU_CACHE_13_041: [** If LRU Node state is `LRU_NODE_STATE_READY` only then the old Node is removed from list by calling `DList_RemoveEntryList`. **]**

- **SRS_LRU_CACHE_13_043: [** On success, `evict_callback` is called with the status `LRU_CACHE_EVICT_OK` and the evicted item. **]**

- **SRS_LRU_CACHE_13_042: [** `lru_cache_put` shall release the lock in exclusive mode. **]**

**SRS_LRU_CACHE_13_049: [** On success, `lru_cache_put` shall return `LRU_CACHE_PUT_OK`. **]**

**SRS_LRU_CACHE_13_050: [** For any other errors, `lru_cache_put` shall return `LRU_CACHE_PUT_ERROR` **]**


### lru_cache_get

```c
MOCKABLE_FUNCTION(, void*, lru_cache_get, LRU_CACHE_HANDLE, lru_cache, void*, key);
```

Gets the `value` of the `key` from the cache. If the `key` is found, the node is made as tail if it is not already.

**SRS_LRU_CACHE_13_051: [** If `lru_cache` is `NULL`, then `lru_cache_get` shall fail and return `NULL`. **]**

**SRS_LRU_CACHE_13_052: [** If `key` is `NULL`, then `lru_cache_get` shall fail and return `NULL`. **]**

**SRS_LRU_CACHE_13_053: [** `lru_cache_get` shall get `CLDS_HAZARD_POINTERS_THREAD_HANDLE` by calling `clds_hazard_pointers_thread_helper_get_thread`. **]**

**SRS_LRU_CACHE_13_054: [** `lru_cache_get` shall check hash table for any existence of the value by calling `clds_hash_table_find` on the `key`. **]**

**SRS_LRU_CACHE_13_079: [** If the LRU Node is already evicted with the state `LRU_NODE_STATE_EVICTED`, then `lru_cache_get` shall return `CLDS_HASH_TABLE_ITEM` value of the `key`. **]**

**SRS_LRU_CACHE_13_055: [**  If the `key` is found and the node from the `key` is not recently used: **]**

- **SRS_LRU_CACHE_13_056: [** `lru_cache_get` shall acquire the lock in exclusive mode. **]**

- **SRS_LRU_CACHE_13_057: [** `lru_cache_get` shall remove the old value node from `doubly_linked_list` by calling `DList_RemoveEntryList`. **]**

- **SRS_LRU_CACHE_13_058: [** `lru_cache_get` shall make the node as the tail by calling `DList_InsertTailList`. **]**

- **SRS_LRU_CACHE_13_059: [** `lru_cache_get` shall release the lock in exclusive mode. **]**

**SRS_LRU_CACHE_13_060: [** On success, `lru_cache_get` shall return `CLDS_HASH_TABLE_ITEM` value of the `key`. **]**

**SRS_LRU_CACHE_13_061: [** If there are any failures, `lru_cache_get` shall return `NULL`. **]**
