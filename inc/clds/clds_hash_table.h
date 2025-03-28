// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_HASH_TABLE_H
#define CLDS_HASH_TABLE_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "c_pal/thandle.h"

#include "c_util/cancellation_token.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_sorted_list.h"

#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HASH_TABLE_H */
