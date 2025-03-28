// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_SORTED_LIST_H
#define CLDS_SORTED_LIST_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "macro_utils/macro_utils.h"
#include "c_pal/interlocked.h"
#include "clds_hazard_pointers.h"

#include "umock_c/umock_c_prod.h"
#ifdef __cplusplus
extern "C" {
#endif

// handle to the sorted list
typedef struct CLDS_SORTED_LIST_TAG* CLDS_SORTED_LIST_HANDLE;

struct CLDS_SORTED_LIST_ITEM_TAG;

#define CLDS_CONDITION_CHECK_RESULT_VALUES \
    CLDS_CONDITION_CHECK_OK, \
    CLDS_CONDITION_CHECK_ERROR, \
    CLDS_CONDITION_CHECK_NOT_MET

MU_DEFINE_ENUM(CLDS_CONDITION_CHECK_RESULT, CLDS_CONDITION_CHECK_RESULT_VALUES);

typedef void*(*SORTED_LIST_GET_ITEM_KEY_CB)(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item);
typedef int(*SORTED_LIST_KEY_COMPARE_CB)(void* context, void* key1, void* key2);
typedef void(*SORTED_LIST_ITEM_CLEANUP_CB)(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item);
typedef void(*SORTED_LIST_SKIPPED_SEQ_NO_CB)(void* context, int64_t skipped_sequence_no);
typedef CLDS_CONDITION_CHECK_RESULT (*CONDITION_CHECK_CB)(void* context, void* new_key, void* old_key);

// this is the structure needed for one sorted list item
// it contains information like ref count, next pointer, etc.
typedef struct CLDS_SORTED_LIST_ITEM_TAG
{
    // these are internal variables used by the sorted list
    volatile_atomic int32_t ref_count;
    SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    struct CLDS_SORTED_LIST_ITEM_TAG* volatile_atomic next;
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
    CLDS_SORTED_LIST_SET_VALUE_ERROR, \
    CLDS_SORTED_LIST_SET_VALUE_NOT_FOUND, \
    CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET

MU_DEFINE_ENUM(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES);

#define CLDS_SORTED_LIST_GET_COUNT_RESULT_VALUES \
    CLDS_SORTED_LIST_GET_COUNT_OK, \
    CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED, \
    CLDS_SORTED_LIST_GET_COUNT_ERROR

MU_DEFINE_ENUM(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_RESULT_VALUES);

#define CLDS_SORTED_LIST_GET_ALL_RESULT_VALUES \
    CLDS_SORTED_LIST_GET_ALL_OK, \
    CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED, \
    CLDS_SORTED_LIST_GET_ALL_NOT_ENOUGH_SPACE, \
    CLDS_SORTED_LIST_GET_ALL_ERROR

MU_DEFINE_ENUM(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_RESULT_VALUES);

// sorted list API
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_HANDLE, clds_sorted_list_create, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB, get_item_key_cb, void*, get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB, key_compare_cb, void*, key_compare_cb_context, volatile_atomic int64_t*, start_sequence_number, SORTED_LIST_SKIPPED_SEQ_NO_CB, skipped_seq_no_cb, void*, skipped_seq_no_cb_context);
MOCKABLE_FUNCTION(, void, clds_sorted_list_destroy, CLDS_SORTED_LIST_HANDLE, clds_sorted_list);

MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_INSERT_RESULT, clds_sorted_list_insert, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_item, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM*, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_DELETE_RESULT, clds_sorted_list_delete_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_REMOVE_RESULT, clds_sorted_list_remove_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_SORTED_LIST_ITEM**, item, int64_t*, sequence_number);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_find_key, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_SET_VALUE_RESULT, clds_sorted_list_set_value, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_SORTED_LIST_ITEM*, new_item, CONDITION_CHECK_CB, condition_check_func, void*, condition_check_context, CLDS_SORTED_LIST_ITEM**, old_item, int64_t*, sequence_number, bool, only_if_exists);

// Helpers to take a snapshot of the list
MOCKABLE_FUNCTION(, void, clds_sorted_list_lock_writes, CLDS_SORTED_LIST_HANDLE, clds_sorted_list);
MOCKABLE_FUNCTION(, void, clds_sorted_list_unlock_writes, CLDS_SORTED_LIST_HANDLE, clds_sorted_list);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_GET_COUNT_RESULT, clds_sorted_list_get_count, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, uint64_t*, item_count);
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_GET_ALL_RESULT, clds_sorted_list_get_all, CLDS_SORTED_LIST_HANDLE, clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, uint64_t, item_count, CLDS_SORTED_LIST_ITEM**, items, uint64_t*, retrieved_item_count, bool, require_locked_list);

// helper APIs for creating/destroying a sorted list node
MOCKABLE_FUNCTION(, CLDS_SORTED_LIST_ITEM*, clds_sorted_list_node_create, size_t, node_size, SORTED_LIST_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_sorted_list_node_inc_ref, CLDS_SORTED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_sorted_list_node_release, CLDS_SORTED_LIST_ITEM*, item);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_SORTED_LIST_H */
