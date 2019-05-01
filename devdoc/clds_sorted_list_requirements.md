# `clds_sorted_list` requirements

## Overview

`clds_sorted_list` is module that implements a lock free sorted singly linked list.

The module provides the following functionality:
- Inserting items in the list based on a user specified order given by a key compare function
- Delete an item from the list by its previously obtained pointer
- Delete an item from the list by using a custom item compare function
- Find an item in the list by using a custom item compare function
- Replace the value of an item in the list by its key

All operations can be concurrent with other operations of the same or different kind.

## Exposed API

```c
// handle to the sorted list
typedef struct CLDS_SORTED_LIST_TAG* CLDS_SORTED_LIST_HANDLE;

struct CLDS_SORTED_LIST_ITEM_TAG;

typedef void*(*SORTED_LIST_GET_ITEM_KEY_CB)(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item);
typedef int(*SORTED_LIST_KEY_COMPARE_CB)(void* context, void* key1, void* key2);
typedef void(*SORTED_LIST_ITEM_CLEANUP_CB)(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item);
typedef void(*SORTED_LIST_SKIPPED_SEQ_NO_CB)(void* context, int64_t skipped_sequence_no);

// this is the structure needed for one sorted list item
// it contains information like ref count, next pointer, etc.
typedef struct CLDS_SORTED_LIST_ITEM_TAG
{
    // these are internal variables used by the sorted list
    volatile LONG ref_count;
    SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    volatile struct CLDS_SORTED_LIST_ITEM_TAG* next;
    int64_t seq_no;
} CLDS_SORTED_LIST_ITEM;

// these are macros that help declaring a type that can be stored in the sorted list
#define DECLARE_SORTED_LIST_NODE_TYPE(record_type) \
typedef struct MU_C3(SORTED_LIST_NODE_,record_type,_TAG) \
{ \
    CLDS_SORTED_LIST_ITEM item; \
    record_type record; \
} MU_C2(SORTED_LIST_NODE_,record_type); \

#define CLDS_SORTED_LIST_NODE_CREATE(record_type, item_cleanup_callback, item_cleanup_callback_context) \
clds_sorted_list_node_create(sizeof(MU_C2(SORTED_LIST_NODE_,record_type)), item_cleanup_callback, item_cleanup_callback_context)

#define CLDS_SORTED_LIST_NODE_INC_REF(record_type, ptr) \
clds_sorted_list_node_inc_ref(ptr)

#define CLDS_SORTED_LIST_NODE_RELEASE(record_type, ptr) \
clds_sorted_list_node_release(ptr)

#define CLDS_SORTED_LIST_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(MU_C2(SORTED_LIST_NODE_,record_type), record)))

#define CLDS_SORTED_LIST_INSERT_RESULT_VALUES \
    CLDS_SORTED_LIST_INSERT_OK, \
    CLDS_SORTED_LIST_INSERT_ERROR, \
    CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS

MU_DEFINE_ENUM(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);

#define CLDS_SORTED_LIST_DELETE_RESULT_VALUES \
    CLDS_SORTED_LIST_DELETE_OK, \
    CLDS_SORTED_LIST_DELETE_ERROR, \
    CLDS_SORTED_LIST_DELETE_NOT_FOUND

MU_DEFINE_ENUM(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);

#define CLDS_SORTED_LIST_REMOVE_RESULT_VALUES \
    CLDS_SORTED_LIST_REMOVE_OK, \
    CLDS_SORTED_LIST_REMOVE_ERROR, \
    CLDS_SORTED_LIST_REMOVE_NOT_FOUND

MU_DEFINE_ENUM(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);

#define CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES \
    CLDS_SORTED_LIST_SET_VALUE_OK, \
    CLDS_SORTED_LIST_SET_VALUE_ERROR

MU_DEFINE_ENUM(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES);

// sorted list API
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_HANDLE, clds_sorted_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB, get_item_key_cb, void*, get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB, key_compare_cb, void*, key_compare_cb_context, volatile int64_t*, start_sequence_number, SORTED_LIST_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context);
MOCKABLE_FUNCTION(, void, clds_sorted_list_destroy, CLDS_SORTED_LIST_HANDLE, clds_sorted_list);

MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_INSERT_RESULT, clds_sorted_list_insert, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_item, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_REMOVE_RESULT, clds_sorted_list_remove_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_SORTED_LIST_ITEM**, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_find_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_SET_VALUE_RESULT, clds_sorted_list_set_value, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, const void*, key, CLDS_SORTED_LIST_ITEM*, new_item, CLDS_SORTED_LIST_ITEM**, old_item, int64_t*, sequence_number);

// helper APIs for creating/destroying a sorted list node
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_node_create, size_t, node_size, SORTED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_sorted_list_node_inc_ref, CLDS_SORTED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_sorted_list_node_release, CLDS_SORTED_LIST_ITEM*, item);
```

### clds_sorted_list_create

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_HANDLE, clds_sorted_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB, get_item_key_cb, void*, get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB, key_compare_cb, void*, key_compare_cb_context, volatile int64_t*, start_sequence_number, SORTED_LIST_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context);
```

**SRS_CLDS_SORTED_LIST_01_001: [** `clds_sorted_list_create` shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. **]**

**SRS_CLDS_SORTED_LIST_01_002: [** If any error happens, `clds_sorted_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_003: [** If `clds_hazard_pointers` is NULL, `clds_sorted_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_045: [** If `get_item_key_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_049: [** `get_item_key_cb_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SORTED_LIST_01_046: [** If `key_compare_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_050: [** `key_compare_cb_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SORTED_LIST_01_058: [** `start_sequence_number` shall be used by the sorted list to compute the sequence number of each operation. **]**

**SRS_CLDS_SORTED_LIST_01_059: [** `start_sequence_number` shall be allowed to be NULL, in which case no order shall be provided for the operations. **]**

**SRS_CLDS_SORTED_LIST_01_076: [** `skipped_seq_no_cb` shall be allowed to be NULL. **]**

**SRS_CLDS_SORTED_LIST_01_077: [** `skipped_seq_no_cb_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SORTED_LIST_01_078: [** If `start_sequence_number` is NULL, then `skipped_seq_no_cb` must also be NULL, otherwise `clds_sorted_list_create` shall fail and return NULL. **]**

### clds_sorted_list_destroy

```c
MOCKABLE_FUNCTION(, void, clds_sorted_list_destroy, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, SORTED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_SORTED_LIST_01_004: [** `clds_sorted_list_destroy` shall free all resources associated with the sorted list instance. **]**

**SRS_CLDS_SORTED_LIST_01_005: [** If `clds_sorted_list` is NULL, `clds_sorted_list_destroy` shall return. **]**

**SRS_CLDS_SORTED_LIST_01_039: [** Any items still present in the list shall be freed. **]**

**SRS_CLDS_SORTED_LIST_01_040: [** For each item that is freed, the callback `item_cleanup_callback` passed to `clds_sorted_list_node_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. **]**

**SRS_CLDS_SORTED_LIST_01_041: [** If `item_cleanup_callback` is NULL, no user callback shall be triggered for the freed items. **]**

### clds_sorted_list_insert

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_INSERT_RESULT, clds_sorted_list_insert, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
```

`clds_sorted_list_insert` inserts an item in the list.

**SRS_CLDS_SORTED_LIST_01_010: [** On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. **]**

**SRS_CLDS_SORTED_LIST_01_011: [** If `clds_sorted_list` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_012: [** If `item` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_013: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_047: [** `clds_sorted_list_insert` shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. **]**

**SRS_CLDS_SORTED_LIST_01_048: [** If the item with the given key already exists in the list, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS`. **]**

**SRS_CLDS_SORTED_LIST_01_079: [** If sequence numbers are generated and a skipped sequence number callback was provided to `clds_sorted_list_create`, when the item is indicated as already existing, the generated sequence number shall be indicated as skipped. **]**

**SRS_CLDS_SORTED_LIST_01_060: [** For each insert the order of the operation shall be computed based on the start sequence number passed to `clds_sorted_list_create`. **]**

**SRS_CLDS_SORTED_LIST_01_069: [** If no start sequence number was provided in `clds_sorted_list_create` and `sequence_number` is NULL, no sequence number computations shall be done. **]**

**SRS_CLDS_SORTED_LIST_01_061: [** If the `sequence_number` argument passed to `clds_sorted_list_insert` is NULL, the computed sequence number for the insert shall still be computed but it shall not be provided to the user. **]**

**SRS_CLDS_SORTED_LIST_01_062: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_sorted_list_create`, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. **]**

### clds_sorted_list_delete

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_item, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
```

**SRS_CLDS_SORTED_LIST_01_014: [** `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. **]**

**SRS_CLDS_SORTED_LIST_01_026: [** On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. **]**

**SRS_CLDS_SORTED_LIST_01_015: [** If `clds_sorted_list` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_016: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_017: [** If `item` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_018: [** If the item does not exist in the list, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. **]**

**SRS_CLDS_SORTED_LIST_01_042: [** When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. **]**

**SRS_CLDS_SORTED_LIST_01_063: [** For each delete the order of the operation shall be computed based on the start sequence number passed to `clds_sorted_list_create`. **]**

**SRS_CLDS_SORTED_LIST_01_070: [** If no start sequence number was provided in `clds_sorted_list_create` and `sequence_number` is NULL, no sequence number computations shall be done. **]**

**SRS_CLDS_SORTED_LIST_01_064: [** If the `sequence_number` argument passed to `clds_sorted_list_delete` is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. **]**

**SRS_CLDS_SORTED_LIST_01_065: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_sorted_list_create`, `clds_sorted_list_delete` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

### clds_sorted_list_delete_key

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, int64_t*, sequence_number);
```

**SRS_CLDS_SORTED_LIST_01_019: [** `clds_sorted_list_delete_key` shall delete an item by its key. **]**

**SRS_CLDS_SORTED_LIST_01_025: [** On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. **]**

**SRS_CLDS_SORTED_LIST_01_020: [** If `clds_sorted_list` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_021: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_022: [** If `key` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_024: [** If the key is not found, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. **]**

**SRS_CLDS_SORTED_LIST_01_066: [** For each delete key the order of the operation shall be computed based on the start sequence number passed to `clds_sorted_list_create`. **]**

**SRS_CLDS_SORTED_LIST_01_071: [** If no start sequence number was provided in `clds_sorted_list_create` and `sequence_number` is NULL, no sequence number computations shall be done. **]**

**SRS_CLDS_SORTED_LIST_01_067: [** If the `sequence_number` argument passed to `clds_sorted_list_delete_key` is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. **]**

**SRS_CLDS_SORTED_LIST_01_068: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_sorted_list_create`, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. **]**

### clds_sorted_list_remove_key

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_REMOVE_RESULT, clds_sorted_list_remove_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_SORTED_LIST_ITEM**, item);
```

**SRS_CLDS_SORTED_LIST_01_051: [** `clds_sorted_list_remove_key` shall delete an item by its key and return the pointer to the deleted item. **]**

**SRS_CLDS_SORTED_LIST_01_052: [** On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. **]**

**SRS_CLDS_SORTED_LIST_01_054: [** On success, the found item shall be returned in the `item` argument. **]**

**SRS_CLDS_SORTED_LIST_01_053: [** If `clds_sorted_list` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_055: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_056: [** If `key` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_057: [** If the key is not found, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_NOT_FOUND`. **]**

**SRS_CLDS_SORTED_LIST_01_072: [** For each remove key the order of the operation shall be computed based on the start sequence number passed to `clds_sorted_list_create`. **]**

**SRS_CLDS_SORTED_LIST_01_073: [** If no start sequence number was provided in `clds_sorted_list_create` and `sequence_number` is NULL, no sequence number computations shall be done. **]**

**SRS_CLDS_SORTED_LIST_01_074: [** If the `sequence_number` argument passed to `clds_sorted_list_remove_key` is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. **]**

**SRS_CLDS_SORTED_LIST_01_075: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_sorted_list_create`, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. **]**

### clds_sorted_list_find_key

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_find_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
```

**SRS_CLDS_SORTED_LIST_01_027: [** `clds_sorted_list_find_key` shall find in the list the first item that matches the criteria given by a user compare function. **]**

**SRS_CLDS_SORTED_LIST_01_029: [** On success `clds_sorted_list_find_key` shall return a non-NULL pointer to the found linked list item. **]**

**SRS_CLDS_SORTED_LIST_01_028: [** If `clds_sorted_list` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_030: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_031: [** If `key` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_033: [** If no item satisfying the user compare function is found in the list, `clds_sorted_list_find_key` shall fail and return NULL. **]**

**SRS_CLDS_SORTED_LIST_01_034: [** `clds_sorted_list_find_key` shall return a pointer to the item with the reference count already incremented so that it can be safely used by the caller. **]**

### clds_sorted_list_set_value

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_SET_VALUE_RESULT, clds_sorted_list_set_value, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, const void*, key, CLDS_SORTED_LIST_ITEM*, new_item, CLDS_SORTED_LIST_ITEM**, old_item, int64_t*, sequence_number);
```

**SRS_CLDS_SORTED_LIST_01_080: [** `clds_sorted_list_set_value` shall replace in the list the item that matches the criteria given by the compare function passed to `clds_sorted_list_create` with `new_item` and on success it shall return `CLDS_SORTED_LIST_SET_VALUE_OK`. **]**

**SRS_CLDS_SORTED_LIST_01_081: [** If `clds_sorted_list` is NULL, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_082: [** If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_083: [** If `key` is NULL, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_084: [** If `new_item` is NULL, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_085: [** If `old_item` is NULL, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_086: [** If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_sorted_list_create`, `clds_sorted_list_set_value` shall fail and return `CLDS_SORTED_LIST_SET_VALUE_ERROR`. **]**

**SRS_CLDS_SORTED_LIST_01_087: [** If the `key` entry does not exist in the list, `new_item` shall be inserted at the `key` position. **]**

**SRS_CLDS_SORTED_LIST_01_088: [** If the `key` entry exists in the list, its value shall be replaced with `new_item`. **]**

**SRS_CLDS_SORTED_LIST_01_089: [** The previous value shall be returned in `old_item`. **]**

**SRS_CLDS_SORTED_LIST_01_090: [** For each set value the order of the operation shall be computed based on the start sequence number passed to `clds_sorted_list_create`. **]**

**SRS_CLDS_SORTED_LIST_01_091: [** If no start sequence number was provided in `clds_sorted_list_create` and `sequence_number` is NULL, no sequence number computations shall be done. **]**

**SRS_CLDS_SORTED_LIST_01_092: [** If the `sequence_number` argument passed to `clds_sorted_list_set_value` is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. **]**

### clds_sorted_list_node_create

```c
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_node_create, size_t, node_size, SORTED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_SORTED_LIST_01_036: [** `item_cleanup_callback` shall be allowed to be NULL. **]**

**SRS_CLDS_SORTED_LIST_01_037: [** `item_cleanup_callback_context` shall be allowed to be NULL. **]**

### clds_sorted_list_node_destroy

```c
MOCKABLE_FUNCTION(, void, clds_sorted_list_node_release, CLDS_SORTED_LIST_ITEM*, item);
```

### Item reclamation

**SRS_CLDS_SORTED_LIST_01_043: [** The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. **]**

**SRS_CLDS_SORTED_LIST_01_044: [** If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. **]**
