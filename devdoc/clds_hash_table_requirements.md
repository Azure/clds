# `clds_hash_table` requirements

## Overview

`clds_hash_table` is module that implements a lockless hash table.

The module provides the following functionality:
- Inserting items in the hash table
- Delete an item from the hash table by its key
- Replace an item in the hash table by its key
- Find an item in the hash table by its key

All operations can be concurrent with other operations of the same or different kind.

This hash table supports taking a snapshot of the current state by blocking all changes to the table and dumping the nodes.

### Future work

The snapshot functionality will be extended in the future so that concurrent operations are possible. This will be done by storing changes during snapshots in a separate structure and then merging after the snapshot completes.

## Exposed API

```c
struct CLDS_HASH_TABLE_ITEM_TAG;

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);
typedef int (*KEY_COMPARE_FUNC)(void* key_1, void* key_2);
typedef void(*HASH_TABLE_ITEM_CLEANUP_CB)(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item);
typedef void(*HASH_TABLE_SKIPPED_SEQ_NO_CB)(void* context, int64_t skipped_sequence_no);

typedef struct HASH_TABLE_ITEM_TAG
{
    // these are internal variables used by the hash table
    HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    void* key;
} HASH_TABLE_ITEM;

DECLARE_SORTED_LIST_NODE_TYPE(HASH_TABLE_ITEM)

typedef struct SORTED_LIST_NODE_HASH_TABLE_ITEM_TAG CLDS_HASH_TABLE_ITEM;

// these are macros that help declaring a type that can be stored in the hash table
#define DECLARE_HASH_TABLE_NODE_TYPE(record_type) \
typedef struct MU_C3(HASH_TABLE_NODE_,record_type,_TAG) \
{ \
    CLDS_HASH_TABLE_ITEM list_item; \
    record_type record; \
} MU_C2(HASH_TABLE_NODE_,record_type); \

#define CLDS_HASH_TABLE_NODE_CREATE(record_type, item_cleanup_callback, item_cleanup_callback_context) \
clds_hash_table_node_create(sizeof(MU_C2(HASH_TABLE_NODE_,record_type)), item_cleanup_callback, item_cleanup_callback_context)

#define CLDS_HASH_TABLE_NODE_INC_REF(record_type, ptr) \
clds_hash_table_node_inc_ref(ptr)

#define CLDS_HASH_TABLE_NODE_RELEASE(record_type, ptr) \
clds_hash_table_node_release(ptr)

#define CLDS_HASH_TABLE_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(MU_C2(HASH_TABLE_NODE_,record_type), record)))

#define CLDS_HASH_TABLE_INSERT_RESULT_VALUES \
    CLDS_HASH_TABLE_INSERT_OK, \
    CLDS_HASH_TABLE_INSERT_ERROR, \
    CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS

MU_DEFINE_ENUM(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);

#define CLDS_HASH_TABLE_DELETE_RESULT_VALUES \
    CLDS_HASH_TABLE_DELETE_OK, \
    CLDS_HASH_TABLE_DELETE_ERROR, \
    CLDS_HASH_TABLE_DELETE_NOT_FOUND

MU_DEFINE_ENUM(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);

#define CLDS_HASH_TABLE_REMOVE_RESULT_VALUES \
    CLDS_HASH_TABLE_REMOVE_OK, \
    CLDS_HASH_TABLE_REMOVE_ERROR, \
    CLDS_HASH_TABLE_REMOVE_NOT_FOUND

MU_DEFINE_ENUM(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);

#define CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES \
    CLDS_HASH_TABLE_SET_VALUE_OK, \
    CLDS_HASH_TABLE_SET_VALUE_ERROR, \
    CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET

MU_DEFINE_ENUM(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);

#define CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES \
    CLDS_HASH_TABLE_SNAPSHOT_OK, \
    CLDS_HASH_TABLE_SNAPSHOT_ERROR, \
    CLDS_HASH_TABLE_SNAPSHOT_ABANDONED

MU_DEFINE_ENUM(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, volatile_atomic int64_t*, start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_INSERT_RESULT, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete_key_value, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_REMOVE_RESULT, clds_hash_table_remove, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM**, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_SET_VALUE_RESULT, clds_hash_table_set_value, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, new_item, CONDITION_CHECK_CB, condition_check_func, void*, condition_check_context, CLDS_HASH_TABLE_ITEM**, old_item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_find, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_SNAPSHOT_RESULT, clds_hash_table_snapshot, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_HASH_TABLE_ITEM***, items, uint64_t*, item_count, THANDLE(CANCELLATION_TOKEN), cancellation_token);

// helper APIs for creating/destroying a hash table node
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_node_create, size_t, node_size, HASH_TABLE_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_hash_table_node_inc_ref, CLDS_HASH_TABLE_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_hash_table_node_release, CLDS_HASH_TABLE_ITEM*, item);
```

### clds_hash_table_create

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, volatile_atomic int64_t*, start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context);
```

**SRS_CLDS_HASH_TABLE_01_001: [** `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. **]**

**SRS_CLDS_HASH_TABLE_01_002: [** If any error happens, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_003: [** If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_045: [** If `key_compare_func` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_004: [** If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_005: [** If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_057: [** `start_sequence_number` shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. **]**

**SRS_CLDS_HASH_TABLE_01_058: [** `start_sequence_number` shall be allowed to be NULL, in which case no sequence number computations shall be performed. **]**

**SRS_CLDS_HASH_TABLE_01_027: [** The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. **]**

**S_R_S_CLDS_HASH_TABLE_01_072: [** `skipped_seq_no_cb` shall be allowed to be NULL. **]**

**S_R_S_CLDS_HASH_TABLE_01_073: [** `skipped_seq_no_cb_context` shall be allowed to be NULL. **]**

**S_R_S_CLDS_HASH_TABLE_01_074: [** If `start_sequence_number` is NULL, then `skipped_seq_no_cb` must also be NULL, otherwise `clds_sorted_list_create` shall fail and return NULL. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
```

**SRS_CLDS_HASH_TABLE_01_006: [** `clds_hash_table_destroy` shall free all resources associated with the hash table instance. **]**

**SRS_CLDS_HASH_TABLE_01_007: [** If `clds_hash_table` is NULL, `clds_hash_table_destroy` shall return. **]**

### clds_hash_table_insert

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_INSERT_RESULT, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t*, sequence_number);
```

`clds_hash_table_insert` inserts a key/value pair in the hash table.

**SRS_CLDS_HASH_TABLE_01_009: [** On success `clds_hash_table_insert` shall return `CLDS_HASH_TABLE_INSERT_OK`. **]**

**SRS_CLDS_HASH_TABLE_01_010: [** If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_011: [** If `key` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_012: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_032: [** `clds_hash_table_insert` shall try the following until it acquires a write lock for the table: **]**

 - **SRS_CLDS_HASH_TABLE_42_033: [** `clds_hash_table_insert` shall increment the count of pending write operations. **]**

 - **SRS_CLDS_HASH_TABLE_42_034: [** If the counter to lock the table for writes is non-zero then: **]**

   - **SRS_CLDS_HASH_TABLE_42_035: [** `clds_hash_table_insert` shall decrement the count of pending write operations. **]**

   - **SRS_CLDS_HASH_TABLE_42_036: [** `clds_hash_table_insert` shall wait for the counter to lock the table for writes to reach 0 and repeat. **]**

**SRS_CLDS_HASH_TABLE_01_038: [** `clds_hash_table_insert` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**S_R_S_CLDS_HASH_TABLE_01_096: [** `clds_hash_table_insert` shall attempt to find the key `key` in all the arrays of buckets except the top level array of buckets one. **]**

**S_R_S_CLDS_HASH_TABLE_01_097: [** If the key is not found then `clds_hash_table_insert` shall proceed to insert the key/value pair in the top level array of buckets. **]**

**S_R_S_CLDS_HASH_TABLE_01_098: [** If the top level array of buckets has changed while looking up the key, the key lookup shall be restarted. **]**

**SRS_CLDS_HASH_TABLE_01_018: [** `clds_hash_table_insert` shall obtain the bucket index to be used by calling `compute_hash` and passing to it the `key` value. **]**

**SRS_CLDS_HASH_TABLE_01_019: [** If no sorted list exists at the determined bucket index then a new list shall be created. **]**

**SRS_CLDS_HASH_TABLE_01_071: [** When a new list is created, the start sequence number passed to `clds_hash_tabel_create` shall be passed as the `start_sequence_number` argument. **]**

**SRS_CLDS_HASH_TABLE_01_020: [** A new sorted list item shall be created by calling `clds_sorted_list_node_create`. **]**

**SRS_CLDS_HASH_TABLE_01_021: [** The new sorted list node shall be inserted in the sorted list at the identified bucket by calling `clds_sorted_list_insert`. **]**

**SRS_CLDS_HASH_TABLE_01_022: [** If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_046: [** If the key already exists in the hash table, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ALREADY_EXISTS`. **]**

**SRS_CLDS_HASH_TABLE_01_030: [** If the number of items in the list reaches the number of buckets, the number of buckets shall be doubled. **]**

**S_R_S_CLDS_HASH_TABLE_01_031: [** When the number of buckets is doubled a new array of buckets shall be allocated and added to the list of array of buckets. **]**

**S_R_S_CLDS_HASH_TABLE_01_032: [** All new inserts shall be done to this new array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_059: [** For each insert the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_insert`. **]**

**SRS_CLDS_HASH_TABLE_01_062: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_063: [** `clds_hash_table_insert` shall decrement the count of pending write operations. **]**

### clds_hash_table_delete

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, int64_t*, sequence_number);
```

`clds_hash_table_delete` deletes a key from the hash table.

**SRS_CLDS_HASH_TABLE_01_039: [** `clds_hash_table_delete` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**SRS_CLDS_HASH_TABLE_01_014: [** On success `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_OK`. **]**

**SRS_CLDS_HASH_TABLE_01_015: [** If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_017: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_016: [** If `key` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_037: [** `clds_hash_table_delete` shall try the following until it acquires a write lock for the table: **]**

 - **SRS_CLDS_HASH_TABLE_42_038: [** `clds_hash_table_delete` shall increment the count of pending write operations. **]**

 - **SRS_CLDS_HASH_TABLE_42_039: [** If the counter to lock the table for writes is non-zero then: **]**

   - **SRS_CLDS_HASH_TABLE_42_040: [** `clds_hash_table_delete` shall decrement the count of pending write operations. **]**

   - **SRS_CLDS_HASH_TABLE_42_041: [** `clds_hash_table_delete` shall wait for the counter to lock the table for writes to reach 0 and repeat. **]**

**SRS_CLDS_HASH_TABLE_01_101: [** Otherwise, `key` shall be looked up in each of the arrays of buckets starting with the first. **]**

**SRS_CLDS_HASH_TABLE_01_023: [** If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_NOT_FOUND`. **]**

**SRS_CLDS_HASH_TABLE_01_024: [** If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_025: [** If the element to be deleted is not found in an array of buckets, then it shall be looked up in the next available array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_063: [** For each delete the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_delete_key`. **]**

**SRS_CLDS_HASH_TABLE_01_066: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_042: [** `clds_hash_table_insert` shall decrement the count of pending write operations. **]**

### clds_hash_table_delete_key_value

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete_key_value, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value, int64_t*, sequence_number);
```

`clds_hash_table_delete_key_value` deletes a key from the hash table as long as the `value` pointer matches.

**SRS_CLDS_HASH_TABLE_42_001: [** `clds_hash_table_delete_key_value` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**SRS_CLDS_HASH_TABLE_42_002: [** On success `clds_hash_table_delete_key_value` shall return `CLDS_HASH_TABLE_DELETE_OK`. **]**

**SRS_CLDS_HASH_TABLE_42_003: [** If `clds_hash_table` is `NULL`, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_004: [** If `clds_hazard_pointers_thread` is `NULL`, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_005: [** If `key` is `NULL`, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_006: [** If `value` is `NULL`, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_043: [** `clds_hash_table_delete_key_value` shall try the following until it acquires a write lock for the table: **]**

 - **SRS_CLDS_HASH_TABLE_42_044: [** `clds_hash_table_delete_key_value` shall increment the count of pending write operations. **]**

 - **SRS_CLDS_HASH_TABLE_42_045: [** If the counter to lock the table for writes is non-zero then: **]**

   - **SRS_CLDS_HASH_TABLE_42_046: [** `clds_hash_table_delete_key_value` shall decrement the count of pending write operations. **]**

   - **SRS_CLDS_HASH_TABLE_42_047: [** `clds_hash_table_delete_key_value` shall wait for the counter to lock the table for writes to reach 0 and repeat. **]**

**SRS_CLDS_HASH_TABLE_42_007: [** Otherwise, `key` shall be looked up in each of the arrays of buckets starting with the first. **]**

**SRS_CLDS_HASH_TABLE_42_008: [** If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_delete_key_value` shall return `CLDS_HASH_TABLE_DELETE_NOT_FOUND`. **]**

**SRS_CLDS_HASH_TABLE_42_009: [** If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_010: [** If the element to be deleted is not found in an array of buckets, then `clds_hash_table_delete_key_value` shall look in the next available array of buckets. **]**

**SRS_CLDS_HASH_TABLE_42_011: [** For each delete the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_delete_item`. **]**

**SRS_CLDS_HASH_TABLE_42_012: [** If the `sequence_number` argument is non-`NULL`, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_delete_key_value` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_048: [** `clds_hash_table_delete_key_value` shall decrement the count of pending write operations. **]**

### clds_hash_table_remove

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_REMOVE_RESULT, clds_hash_table_remove, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM**, item, int64_t*, sequence_number);
```

**SRS_CLDS_HASH_TABLE_01_047: [** `clds_hash_table_remove` shall remove a key from the hash table and return a pointer to the item to the user. **]**

**SRS_CLDS_HASH_TABLE_01_048: [** `clds_hash_table_remove` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**SRS_CLDS_HASH_TABLE_01_049: [** On success `clds_hash_table_remove` shall return `CLDS_HASH_TABLE_REMOVE_OK`. **]**

**SRS_CLDS_HASH_TABLE_01_050: [** If `clds_hash_table` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_052: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_051: [** If `key` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_056: [** If `item` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_049: [** `clds_hash_table_remove` shall try the following until it acquires a write lock for the table: **]**

 - **SRS_CLDS_HASH_TABLE_42_050: [** `clds_hash_table_remove` shall increment the count of pending write operations. **]**

 - **SRS_CLDS_HASH_TABLE_42_051: [** If the counter to lock the table for writes is non-zero then: **]**

   - **SRS_CLDS_HASH_TABLE_42_052: [** `clds_hash_table_remove` shall decrement the count of pending write operations. **]**

   - **SRS_CLDS_HASH_TABLE_42_053: [** `clds_hash_table_remove` shall wait for the counter to lock the table for writes to reach 0 and repeat. **]**

**SRS_CLDS_HASH_TABLE_01_053: [** If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_remove` shall return `CLDS_HASH_TABLE_REMOVE_NOT_FOUND`. **]**

**SRS_CLDS_HASH_TABLE_01_054: [** If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_055: [** If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_067: [** For each remove the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_remove`. **]**

**SRS_CLDS_HASH_TABLE_01_070: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_054: [** `clds_hash_table_remove` shall decrement the count of pending write operations. **]**

### clds_hash_table_set_value

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_SET_VALUE_RESULT, clds_hash_table_set_value, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, new_item, CONDITION_CHECK_CB, condition_check_func, void*, condition_check_context, CLDS_HASH_TABLE_ITEM**, old_item, int64_t*, sequence_number);
```

**S_R_S_CLDS_HASH_TABLE_01_077: [** `clds_hash_table_set_value` shall set a key value in the hash table and on success it shall return `CLDS_HASH_TABLE_SET_VALUE_OK`. **]**

**S_R_S_CLDS_HASH_TABLE_01_078: [** `clds_hash_table_set_value` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**SRS_CLDS_HASH_TABLE_01_079: [** If `clds_hash_table` is NULL, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_080: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_081: [** If `key` is NULL, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_082: [** If `new_item` is NULL, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_083: [** If `old_item` is NULL, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_01_084: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_055: [** `clds_hash_table_set_value` shall try the following until it acquires a write lock for the table: **]**

 - **SRS_CLDS_HASH_TABLE_42_056: [** `clds_hash_table_set_value` shall increment the count of pending write operations. **]**

 - **SRS_CLDS_HASH_TABLE_42_057: [** If the counter to lock the table for writes is non-zero then: **]**

   - **SRS_CLDS_HASH_TABLE_42_058: [** `clds_hash_table_set_value` shall decrement the count of pending write operations. **]**

   - **SRS_CLDS_HASH_TABLE_42_059: [** `clds_hash_table_set_value` shall wait for the counter to lock the table for writes to reach 0 and repeat. **]**

**SRS_CLDS_HASH_TABLE_01_085: [** `clds_hash_table_set_value` shall go through all non top level bucket arrays and: **]**

- **SRS_CLDS_HASH_TABLE_01_107: [** If there is no sorted list in the bucket identified by the hash of the key, `clds_hash_table_set_value` shall advance to the next level of buckets. **]**

- **SRS_CLDS_HASH_TABLE_01_108: [** If there is a sorted list in the bucket identified by the hash of the key, `clds_hash_table_set_value` shall find the key in the list. **]**

- **SRS_CLDS_HASH_TABLE_01_109: [** If the key is not found, `clds_hash_table_set_value` shall advance to the next level of buckets. **]**

- **SRS_CLDS_HASH_TABLE_01_110: [** If the key is found, `clds_hash_table_set_value` shall call `clds_sorted_list_set_value` with the `key`, `new_item`, `condition_check_func`, `condition_check_context` and `old_item` and `only_if_exists` set to `true`. **]**

  - **SRS_CLDS_HASH_TABLE_01_111: [** If `clds_sorted_list_set_value` fails, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

  - **SRS_CLDS_HASH_TABLE_04_001: [** If `clds_sorted_list_set_value` returns `CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET`, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET`. **]**

  - **SRS_CLDS_HASH_TABLE_01_112: [** If `clds_sorted_list_set_value` succeeds, `clds_hash_table_set_value` shall return `CLDS_HASH_TABLE_SET_VALUE_OK`. **]**

**SRS_CLDS_HASH_TABLE_01_102: [** If the key is not found in any of the non top level buckets arrays, `clds_hash_table_set_value`: **]**

- **SRS_CLDS_HASH_TABLE_01_103: [** `clds_hash_table_set_value` shall obtain the sorted list at the bucket corresponding to the hash of the key. **]**

- **SRS_CLDS_HASH_TABLE_01_104: [**  If no list exists at the designated bucket, one shall be created. **]**

- **SRS_CLDS_HASH_TABLE_01_105: [** `clds_hash_table_set_value` shall call `clds_hash_table_set_value` on the top level bucket array, passing `key`, `new_item`, `condition_check_func`, `condition_check_context`, `old_item` and `only_if_exists` set to `false`. **]**

- **SRS_CLDS_HASH_TABLE_01_099: [** If `clds_sorted_list_set_value` returns `CLDS_SORTED_LIST_SET_VALUE_OK`, `clds_hash_table_set_value` shall succeed and return `CLDS_HASH_TABLE_SET_VALUE_OK`. **]**

- **SRS_CLDS_HASH_TABLE_04_002: [** If `clds_sorted_list_set_value` returns `CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET`, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET`. **]**

- **SRS_CLDS_HASH_TABLE_01_100: [** If `clds_sorted_list_set_value` returns any other value, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_060: [** `clds_hash_table_set_value` shall decrement the count of pending write operations. **]**

**SRS_CLDS_HASH_TABLE_01_106: [** If any error occurs, `clds_hash_table_set_value` shall fail and return `CLDS_HASH_TABLE_SET_VALUE_ERROR`. **]**

### clds_hash_table_find

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_find, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
```

**SRS_CLDS_HASH_TABLE_01_034: [** `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. **]**

**SRS_CLDS_HASH_TABLE_01_035: [** If `clds_hash_table` is NULL, `clds_hash_table_find` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_036: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_find` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_037: [** If `key` is NULL, `clds_hash_table_find` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_040: [** `clds_hash_table_find` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. **]**

**SRS_CLDS_HASH_TABLE_01_041: [** `clds_hash_table_find` shall look up the key in the biggest array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_042: [** If the key is not found in the biggest array of buckets, the next bucket arrays shall be looked up. **]**

**SRS_CLDS_HASH_TABLE_01_043: [** If the key is not found at all, `clds_hash_table_find` shall return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_044: [** Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling `clds_sorted_list_find`. **]**

### on_sorted_list_skipped_seq_no

```c
static void on_sorted_list_skipped_seq_no(void* context, int64_t skipped_sequence_no)
```

**SRS_CLDS_HASH_TABLE_01_075: [** `on_sorted_list_skipped_seq_no` called with NULL `context` shall return. **]**

**SRS_CLDS_HASH_TABLE_01_113: [** If the sequence number callback passed to `clds_hash_table_create` was `NULL`, `on_sorted_list_skipped_seq_no` shall return. **]**

**SRS_CLDS_HASH_TABLE_01_076: [** `on_sorted_list_skipped_seq_no` shall call the skipped sequence number callback passed to `clds_hash_table_create` and pass the `skipped_sequence_no` as `skipped_sequence_no` argument. **]**

### clds_hash_table_snapshot

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_SNAPSHOT_RESULT, clds_hash_table_snapshot, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_HASH_TABLE_ITEM***, items, uint64_t*, item_count, THANDLE(CANCELLATION_TOKEN), cancellation_token);
```

`clds_hash_table_snapshot` locks the table for writes and collects all of the items in the table into an array then unlocks the table. During this call, `clds_hash_table_find` will continue to work, but other APIs will block.

**SRS_CLDS_HASH_TABLE_42_013: [** If `clds_hash_table` is `NULL` then `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_014: [** If `clds_hazard_pointers_thread` is `NULL` then `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_015: [** If `items` is `NULL` then `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_016: [** If `item_count` is `NULL` then `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_017: [** `clds_hash_table_snapshot` shall increment a counter to lock the table for writes. **]**

**SRS_CLDS_HASH_TABLE_42_018: [** `clds_hash_table_snapshot` shall wait for the ongoing write operations to complete. **]**

**SRS_CLDS_HASH_TABLE_01_114: [** `clds_hash_table_snapshot` shall determine all the items in the hash table by summing up the item count for all bucket arrays in all levels. **]**

**SRS_CLDS_HASH_TABLE_42_064: [** If there are no items then `clds_hash_table_snapshot` shall set `items` to `NULL` and `item_count` to `0` and return `CLDS_HASH_TABLE_SNAPSHOT_OK`. **]**

**SRS_CLDS_HASH_TABLE_42_023: [** `clds_hash_table_snapshot` shall allocate an array of `CLDS_HASH_TABLE_ITEM*` **]**

**SRS_CLDS_HASH_TABLE_42_024: [** For each bucket in the array: **]**

 - **SRS_CLDS_HASH_TABLE_01_115: [** If `cancellation_token` is non-`NULL` and `cancellation_token_is_cancelled` returns `true` for `cancellation_token`, `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ABANDONED`. **]**

 - **SRS_CLDS_HASH_TABLE_42_026: [** `clds_hash_table_snapshot` shall call `clds_sorted_list_get_all` with the next portion of the allocated array and `false` as `required_locked_list`. **]**

**SRS_CLDS_HASH_TABLE_42_028: [** `clds_hash_table_snapshot` shall store the allocated array of items in `items`. **]**

**SRS_CLDS_HASH_TABLE_42_029: [** `clds_hash_table_snapshot` shall store the count of items in `item_count`. **]**

**SRS_CLDS_HASH_TABLE_42_030: [** `clds_hash_table_snapshot` shall decrement the counter to unlock the table for writes. **]**

**SRS_CLDS_HASH_TABLE_42_061: [** If there are any other failures then `clds_hash_table_snapshot` shall fail and return `CLDS_HASH_TABLE_SNAPSHOT_ERROR`. **]**

**SRS_CLDS_HASH_TABLE_42_031: [** `clds_hash_table_snapshot` shall succeed and return `CLDS_HASH_TABLE_SNAPSHOT_OK`. **]**
