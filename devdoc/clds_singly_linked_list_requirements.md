# clds_singly_linked_list requirements

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

// singly linked list API
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);

MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_delete_if, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_find, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);

// helper APIs for creating/destroying a singly linked list node
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_node_create, size_t, node_size, size_t, item_offset, size_t, record_offset);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_node_destroy, CLDS_SINGLY_LINKED_LIST_ITEM*, item);


### clds_singly_linked_list_create

```c
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_001: [** `clds_singly_linked_list_create` shall create a new singly linked list object and on success it shall return a non-NULL handle to the newly created list. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_002: [** If any error happens, `clds_singly_linked_list_create` shall fail and return NULL. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_003: [** If `clds_hazard_pointers` is NULL, `clds_singly_linked_list_create` shall fail and return NULL. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_004: [** `clds_singly_linked_list_destroy` shall free all resources associated with the hazard pointers instance. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_005: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_destroy` shall return. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_006: [** If `item_cleanup_callback` is NULL, no cleanup callbacks shall be triggered for the items that are in the list when `clds_singly_linked_list_destroy` is called. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_007: [** If `item_cleanup_callback` is not NULL, for each item in the list, `item_cleanup_callback` shall be called in order to allow cleaning up resources by the caller. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_008: [** `item_cleanup_callback_context` shall be allowed to be NULL regardless of `item_cleanup_callback` being NULL or not. **]**

### clds_singly_linked_list_insert

```c
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```

**SRS_CLDS_SINGLY_LINKED_LIST_01_009: [** `clds_singly_linked_list_insert` inserts an item in the list. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_010: [** On success `clds_singly_linked_list_insert` shall return 0. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_011: [** If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_012: [** If `item` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_SINGLY_LINKED_LIST_01_013: [** If `clds_hazard_pointers_thread` is NULL, `clds_singly_linked_list_insert` shall fail and return a non-zero value. **]**

### clds_singly_linked_list_delete

```c
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```