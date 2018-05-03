// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_SORTED_LIST_H
#define CLDS_SORTED_LIST_H

#include "windows.h"
#include "azure_c_shared_utility/umock_c_prod.h"
#include "macro_utils.h"
#include "clds_hazard_pointers.h"

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#include <stdbool.h>
#endif

// handle to the sorted list
typedef struct CLDS_SORTED_LIST_TAG* CLDS_SORTED_LIST_HANDLE;

struct CLDS_SORTED_LIST_ITEM_TAG;

typedef void*(*SORTED_LIST_GET_ITEM_KEY_CB)(struct CLDS_SORTED_LIST_ITEM_TAG* item);
typedef int(*SORTED_LIST_KEY_COMPARE_CB)(void* key1, void* key2);
typedef void(*SORTED_LIST_ITEM_CLEANUP_CB)(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item);

// this is the structure needed for one singly linked list item
// it contains information like ref count, next pointer, etc.
typedef struct CLDS_SORTED_LIST_ITEM_TAG
{
    // these are internal variables used by the singly linked list
    volatile LONG ref_count;
    SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    volatile struct CLDS_SORTED_LIST_ITEM_TAG* next;
} CLDS_SORTED_LIST_ITEM;

// these are macros that help declaring a type that can be stored in the singly linked list
#define DECLARE_SORTED_LIST_NODE_TYPE(record_type) \
typedef struct C3(SORTED_LIST_NODE_,record_type,_TAG) \
{ \
    CLDS_SORTED_LIST_ITEM item; \
    record_type record; \
} C2(SORTED_LIST_NODE_,record_type); \

#define CLDS_SORTED_LIST_NODE_CREATE(record_type, item_cleanup_callback, item_cleanup_callback_context) \
clds_sorted_list_node_create(sizeof(C2(SORTED_LIST_NODE_,record_type)), item_cleanup_callback, item_cleanup_callback_context)

#define CLDS_SORTED_LIST_NODE_INC_REF(record_type, ptr) \
clds_sorted_list_node_inc_ref(ptr)

#define CLDS_SORTED_LIST_NODE_RELEASE(record_type, ptr) \
clds_sorted_list_node_release(ptr)

#define CLDS_SORTED_LIST_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(SORTED_LIST_NODE_,record_type), record)))

#define CLDS_SORTED_LIST_DELETE_RESULT_VALUES \
    CLDS_SORTED_LIST_DELETE_OK, \
    CLDS_SORTED_LIST_DELETE_ERROR, \
    CLDS_SORTED_LIST_DELETE_NOT_FOUND

DEFINE_ENUM(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);

// singly linked list API
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_HANDLE, clds_sorted_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB, get_item_key_cb, SORTED_LIST_KEY_COMPARE_CB, key_compare_cb);
MOCKABLE_FUNCTION(, void, clds_sorted_list_destroy, CLDS_SORTED_LIST_HANDLE, clds_sorted_list);

MOCKABLE_FUNCTION(, int, clds_sorted_list_insert, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_item, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_find_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);

// helper APIs for creating/destroying a singly linked list node
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_node_create, size_t, node_size, SORTED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_sorted_list_node_inc_ref, CLDS_SORTED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_sorted_list_node_release, CLDS_SORTED_LIST_ITEM*, item);
#ifdef __cplusplus
}
#endif

#endif /* CLDS_SORTED_LIST_H */
