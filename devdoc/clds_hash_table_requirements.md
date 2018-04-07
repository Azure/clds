# `clds_hash_table` requirements

## Overview

`clds_hash_table` is module that implements a hash table.
The module provides the following functionality:
- Inserting items in the hash table
- Delete an item from the hash table by its key

All operations can be concurrent with other operations of the same or different kind.

No uniqueness in enforced yet on the hash table.
That means that multiple keys could be inserted succesfully.
When deleting a key, the first found key would be deleted.

## Exposed API

```c
struct CLDS_HASH_TABLE_ITEM_TAG;

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);
typedef void(*HASH_TABLE_ITEM_CLEANUP_CB)(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item);

// it contains information like ref count, next pointer, etc.
typedef struct CLDS_HASH_TABLE_ITEM_TAG
{
    // these are internal variables used by the singly linked list
    size_t record_offset;
    volatile LONG ref_count;
    HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    volatile struct CLDS_HASH_TABLE_ITEM_TAG* next;
} CLDS_HASH_TABLE_ITEM;

// these are macros that help declaring a type that can be stored in the hash table
#define DECLARE_HASH_TABLE_NODE_TYPE(record_type) \
typedef struct C3(HASH_TABLE_NODE_,record_type,_TAG) \
{ \
    CLDS_HASH_TABLE_ITEM item; \
    record_type record; \
} C2(HASH_TABLE_NODE_,record_type); \

#define CLDS_HASH_TABLE_NODE_CREATE(record_type) \
clds_hash_table_node_create(sizeof(C2(SINGLY_LINKED_LIST_NODE_,record_type)), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), item), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record));

#define CLDS_HASH_TABLE_NODE_DESTROY(record_type, ptr) \
clds_hash_table_node_destroy(ptr);

#define CLDS_HASH_TABLE_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record)))

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, void*, value, HASH_TABLE_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);

// helper APIs for creating/destroying a hash table node
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_node_create, size_t, node_size, size_t, item_offset, size_t, record_offset);
MOCKABLE_FUNCTION(, void, clds_hash_table_node_destroy, CLDS_HASH_TABLE_ITEM*, item);
```

### clds_hash_table_create

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**SRS_CLDS_HASH_TABLE_01_001: [** `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. **]**

**SRS_CLDS_HASH_TABLE_01_002: [** If any error happens, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_003: [** If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_004: [** If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_005: [** If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_027: [** The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
```

**SRS_CLDS_HASH_TABLE_01_006: [** `clds_hash_table_destroy` shall free all resources associated with the hash table instance. **]**

**SRS_CLDS_HASH_TABLE_01_007: [** If `clds_hash_table` is NULL, `clds_hash_table_destroy` shall return. **]**

### clds_hash_table_insert

```c
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, void*, value, HASH_TABLE_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_HASH_TABLE_01_008: [** `clds_hash_table_insert` shall insert a key/value pair in the hash table. **]**

**SRS_CLDS_HASH_TABLE_01_009: [** On success `clds_hash_table_insert` shall return 0. **]**

**SRS_CLDS_HASH_TABLE_01_010: [** If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_011: [** If `key` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_012: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_028: [** `item_cleanup_callback` shall be allowed to be NULL. **]**

**SRS_CLDS_HASH_TABLE_01_029: [** `item_cleanup_callback_context` shall be allowed to be NULL. **]**

**SRS_CLDS_HASH_TABLE_01_018: [** `clds_hash_table_insert` shall obtain the bucket index to be used by calling `compute_hash` and passing to it the `key` value. **]**

**SRS_CLDS_HASH_TABLE_01_019: [** If no singly linked list exists at the determined bucket index then a new list shall be created. **]**

**SRS_CLDS_HASH_TABLE_01_020: [** A new singly linked list item shall be created by calling `clds_singly_linked_list_node_create`. **]**

**SRS_CLDS_HASH_TABLE_01_021: [** The new singly linked list node shall be inserted in the singly linked list at the identified bucket by calling `clds_singly_linked_list_insert`. **]**

**SRS_CLDS_HASH_TABLE_01_022: [** If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_030: [** If the number of items in the list reaches the number of buckets, the number of buckets shall be doubled. **]**

**SRS_CLDS_HASH_TABLE_01_031: [** When the number of buckets is doubled a new array of buckets shall be allocated and added to the list of array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_032: [** All new inserts shall be done to this new array of buckets. **]**

### clds_hash_table_delete

```c
MOCKABLE_FUNCTION(, int, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
```

**SRS_CLDS_HASH_TABLE_01_013: [** `clds_hash_table_delete` shall delete a key from the hash table. **]**

**SRS_CLDS_HASH_TABLE_01_014: [** On success `clds_hash_table_delete` shall return 0. **]**

**SRS_CLDS_HASH_TABLE_01_015: [** If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_016: [** If `key` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_017: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_023: [** If the desired key is not found in the hash table, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_024: [** If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_025: [** If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. **]**

**SRS_CLDS_HASH_TABLE_01_026: [** If the array of buckets is found to be empty then it shall be freed and removed from the list of array of buckets. **]**