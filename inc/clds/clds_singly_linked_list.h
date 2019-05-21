// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_SINGLY_LINKED_LIST_H
#define CLDS_SINGLY_LINKED_LIST_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "windows.h"

#include "azure_macro_utils/macro_utils.h"
#include "clds_hazard_pointers.h"

#include "umock_c/umock_c_prod.h"
#ifdef __cplusplus
extern "C" {
#endif

// handle to the singly linked list
typedef struct CLDS_SINGLY_LINKED_LIST_TAG* CLDS_SINGLY_LINKED_LIST_HANDLE;

struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG;

typedef bool(*SINGLY_LINKED_LIST_ITEM_COMPARE_CB)(void* item_compare_context, struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* item);
typedef void(*SINGLY_LINKED_LIST_ITEM_CLEANUP_CB)(void* context, struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* item);

// this is the structure needed for one singly linked list item
// it contains information like ref count, next pointer, etc.
typedef struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG
{
    // these are internal variables used by the singly linked list
    volatile LONG ref_count;
    SINGLY_LINKED_LIST_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    volatile struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* next;
} CLDS_SINGLY_LINKED_LIST_ITEM;

// these are macros that help declaring a type that can be stored in the singly linked list
#define DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(record_type) \
typedef struct MU_C3(SINGLY_LINKED_LIST_NODE_,record_type,_TAG) \
{ \
    CLDS_SINGLY_LINKED_LIST_ITEM item; \
    record_type record; \
} MU_C2(SINGLY_LINKED_LIST_NODE_,record_type); \

#define CLDS_SINGLY_LINKED_LIST_NODE_CREATE(record_type, item_cleanup_callback, item_cleanup_callback_context) \
clds_singly_linked_list_node_create(sizeof(MU_C2(SINGLY_LINKED_LIST_NODE_,record_type)), item_cleanup_callback, item_cleanup_callback_context)

#define CLDS_SINGLY_LINKED_LIST_NODE_INC_REF(record_type, ptr) \
clds_singly_linked_list_node_inc_ref(ptr)

#define CLDS_SINGLY_LINKED_LIST_NODE_RELEASE(record_type, ptr) \
clds_singly_linked_list_node_release(ptr)

#define CLDS_SINGLY_LINKED_LIST_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(MU_C2(SINGLY_LINKED_LIST_NODE_,record_type), record)))

#define CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES \
    CLDS_SINGLY_LINKED_LIST_DELETE_OK, \
    CLDS_SINGLY_LINKED_LIST_DELETE_ERROR, \
    CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND

MU_DEFINE_ENUM(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);

// singly linked list API
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list);

MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, clds_singly_linked_list_delete_if, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_find, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB, item_compare_callback, void*, item_compare_callback_context);

// helper APIs for creating/destroying a singly linked list node
MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_node_create, size_t, node_size, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_node_inc_ref, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_node_release, CLDS_SINGLY_LINKED_LIST_ITEM*, item);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_SINGLY_LINKED_LIST_H */
