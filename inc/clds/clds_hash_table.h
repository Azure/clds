// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_HASH_TABLE_H
#define CLDS_HASH_TABLE_H

#include "windows.h"
#include "azure_c_shared_utility/umock_c_prod.h"
#include "clds_hazard_pointers.h"
#include "clds_sorted_list.h"

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

struct CLDS_HASH_TABLE_ITEM_TAG;

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);
typedef int (*KEY_COMPARE_FUNC)(void* key_1, void* key_2);
typedef void(*HASH_TABLE_ITEM_CLEANUP_CB)(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item);

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
typedef struct C3(HASH_TABLE_NODE_,record_type,_TAG) \
{ \
    CLDS_HASH_TABLE_ITEM list_item; \
    record_type record; \
} C2(HASH_TABLE_NODE_,record_type); \

#define CLDS_HASH_TABLE_NODE_CREATE(record_type, item_cleanup_callback, item_cleanup_callback_context) \
clds_hash_table_node_create(sizeof(C2(HASH_TABLE_NODE_,record_type)), item_cleanup_callback, item_cleanup_callback_context)

#define CLDS_HASH_TABLE_NODE_INC_REF(record_type, ptr) \
clds_hash_table_node_inc_ref(ptr)

#define CLDS_HASH_TABLE_NODE_RELEASE(record_type, ptr) \
clds_hash_table_node_release(ptr)

#define CLDS_HASH_TABLE_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(HASH_TABLE_NODE_,record_type), record)))

#define CLDS_HASH_TABLE_INSERT_RESULT_VALUES \
    CLDS_HASH_TABLE_INSERT_OK, \
    CLDS_HASH_TABLE_INSERT_ERROR, \
    CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS

DEFINE_ENUM(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);

#define CLDS_HASH_TABLE_DELETE_RESULT_VALUES \
    CLDS_HASH_TABLE_DELETE_OK, \
    CLDS_HASH_TABLE_DELETE_ERROR, \
    CLDS_HASH_TABLE_DELETE_NOT_FOUND

DEFINE_ENUM(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_INSERT_RESULT, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_DELETE_RESULT, clds_hash_table_delete_key_value, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_find, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);

// helper APIs for creating/destroying a hash table node
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_node_create, size_t, node_size, HASH_TABLE_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_hash_table_node_inc_ref, CLDS_HASH_TABLE_ITEM*, item);
MOCKABLE_FUNCTION(, void, clds_hash_table_node_release, CLDS_HASH_TABLE_ITEM*, item);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HASH_TABLE_H */
