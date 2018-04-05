# `clds_singly_linked_list` requirements

## Overview

`clds_singly_linked_list` is module that implements a lock free singly linked list.
The module provides the following functionality:
- Inserting items in the list
- Delete an item from the list by its previously obtained pointer
- Delete an item from the list by using a custom item compare function
- Find an item in the list by using a custom item compare function

All operations can be concurrent with other operations of the same or different kind.

## Exposed API

```c
// handle to the singly linked list
typedef struct CLDS_SINGLY_LINKED_LIST_TAG* CLDS_SINGLY_LINKED_LIST_HANDLE;

// this is the structure needed for one singly linked list item
// it contains information like ref count, next pointer, etc.
typedef struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG
{
    size_t record_offset;
    volatile LONG ref_count;
    volatile struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* next;
} CLDS_SINGLY_LINKED_LIST_ITEM;

// these are macros that help declaring a type that can be stored in the singly linked list
#define DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(record_type) \
typedef struct C3(SINGLY_LINKED_LIST_NODE_,record_type,_TAG) \
{ \
    CLDS_SINGLY_LINKED_LIST_ITEM item; \
    record_type record; \
} C2(SINGLY_LINKED_LIST_NODE_,record_type); \

#define CLDS_SINGLY_LINKED_LIST_NODE_CREATE(record_type) \
clds_singly_linked_list_node_create(sizeof(C2(SINGLY_LINKED_LIST_NODE_,record_type)), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), item), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record));

#define CLDS_SINGLY_LINKED_LIST_NODE_DESTROY(record_type, ptr) \
clds_singly_linked_list_node_destroy(ptr);

#define CLDS_SINGLY_LINKED_LIST_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record)))

typedef bool (*SINGLY_LINKED_LIST_ITEM_COMPARE_CB)(void* item_compare_context, CLDS_SINGLY_LINKED_LIST_ITEM* item);
typedef void (*SINGLY_LINKED_LIST_ITEM_CLEANUP_CB)(void* context, CLDS_SINGLY_LINKED_LIST_ITEM* item);

#define DELETE_RESULT_VALUES \
    DELETE_RESULT_OK, \
    DELETE_RESULT_ERROR, \
    DELETE_RESULT_NOT_FOUND

DEFINE_ENUM(DELETE_RESULT, DELETE_RESULT_VALUES);

// singly linked list API
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list);

MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, DELETE_RESULT, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, DELETE_RESULT, clds_singly_linked_list_delete_if, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_find, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);

// helper APIs for creating/destroying a singly linked list node
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_node_create, size_t, node_size, size_t, item_offset, size_t, record_offset);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_node_destroy, CLDS_SINGLY_LINKED_LIST_ITEM*, item);


### clds_singly_linked_list_create

```c
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_001: [** `clds_singly_linked_list_create` shall create a new singly linked list object and on success it shall return a non-NULL handle to the newly created list. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_002: [** If any error happens, `clds_singly_linked_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_003: [** If `clds_hazard_pointers` is NULL, `clds_singly_linked_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_036: [** `item_cleanup_callback` shall be allowed to be NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_037: [** `item_cleanup_callback_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_035: [** `item_cleanup_callback` and `item_cleanup_callback_context` shall be saved in order to be used whenever singly linked list items are reclaimed to allow the user to perform any additional cleanup for each item. **]**

Y**SRS_CLDS_SINGLY_LINKED_LIST_01_038: [** If `item_cleanup_callback` is NULL, no cleanup callbacks shall be triggered for any reclaimed items. **]**

### clds_singly_linked_list_destroy

```c
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_004: [** `clds_singly_linked_list_destroy` shall free all resources associated with the hazard pointers instance. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_005: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_destroy` shall return. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_039: [** Any items still present in the list shall be freed. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_040: [** For each item that is freed, the callback `item_cleanup_callback` passed to `clds_singly_linked_list_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_041: [** If `item_cleanup_callback` is NULL, no user callback shall be triggered for the freed items. **]**

### clds_singly_linked_list_insert

```c
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_009: [** `clds_singly_linked_list_insert` inserts an item in the list. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_010: [** On success `clds_singly_linked_list_insert` shall return 0. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_011: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_012: [** If `item` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_013: [** If `clds_hazard_pointers_thread` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

### clds_singly_linked_list_delete

```c
MOCKABLE_FUNCTION(, DELETE_RESULT, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_014: [** `clds_singly_linked_list_delete` deletes an item from the list by its pointer. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_026: [** On success, `clds_singly_linked_list_delete` shall return `DELETE_RESULT_OK`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_015: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_delete` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_016: [** If `clds_hazard_pointers_thread` is NULL, `clds_singly_linked_list_delete` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_017: [** If `item` is NULL, `clds_singly_linked_list_delete` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_018: [** If the item does not exist in the list, `clds_singly_linked_list_delete` shall fail and return `DELETE_RESULT_NOT_FOUND`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_042: [** When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_043: [** The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_singly_linked_list_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_044: [** If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. **]**

### clds_singly_linked_list_delete_if

```c
MOCKABLE_FUNCTION(, DELETE_RESULT, clds_singly_linked_list_delete_if, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_019: [** `clds_singly_linked_list_delete_if` deletes an item that matches the criteria given by a user compare function. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_025: [** On success, `clds_singly_linked_list_delete_if` shall return `DELETE_RESULT_OK`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_020: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_delete_if` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_021: [** If `clds_hazard_pointers_thread` is NULL, `clds_singly_linked_list_delete_if` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_022: [** If `item_compare_callback` is NULL, `clds_singly_linked_list_delete_if` shall fail and return `DELETE_RESULT_ERROR`. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_023: [** `item_compare_callback_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_024: [** If no item matches the criteria, `clds_singly_linked_list_delete_if` shall fail and return `DELETE_RESULT_NOT_FOUND`. **]**

### clds_singly_linked_list_find

```c
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_find, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_027: [** `clds_singly_linked_list_find` shall find in the list the first item that matches the criteria given by a user compare function. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_029: [** On success `clds_singly_linked_list_find` shall return a non-NULL pointer to the found linked list item. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_028: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_find` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_030: [** If `clds_hazard_pointers_thread` is NULL, `clds_singly_linked_list_find` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_031: [** If `item_compare_callback` is NULL, `clds_singly_linked_list_find` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_032: [** `item_compare_callback_context` shall be allowed to be NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_033: [** If no item satisdying the user compare function is found in the list, `clds_singly_linked_list_find` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_034: [** `clds_singly_linked_list_find` shall return a pointer to the item with the reference ount already incremented so that it can be safely used by the caller. **]**
