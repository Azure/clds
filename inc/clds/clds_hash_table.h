// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_HASH_TABLE_H
#define CLDS_HASH_TABLE_H

#include "windows.h"
#include "azure_c_shared_utility/umock_c_prod.h"
#include "clds_hazard_pointers.h"

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

struct CLDS_HASH_TABLE_ITEM_TAG;

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);
typedef void(*HASH_TABLE_ITEM_CLEANUP_CB)(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item);

struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG;

// it contains information like ref count, next pointer, etc.
typedef struct CLDS_HASH_TABLE_ITEM_TAG
{
    // these are internal variables used by the hash table
    size_t record_offset;
    volatile LONG ref_count;
    HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback;
    void* item_cleanup_callback_context;
    struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* list_item;
} CLDS_HASH_TABLE_ITEM;

// these are macros that help declaring a type that can be stored in the hash table
#define DECLARE_HASH_TABLE_NODE_TYPE(record_type) \
typedef struct C3(HASH_TABLE_NODE_,record_type,_TAG) \
{ \
    CLDS_HASH_TABLE_ITEM item; \
    record_type record; \
} C2(HASH_TABLE_NODE_,record_type); \

#define CLDS_HASH_TABLE_NODE_CREATE(record_type) \
clds_hash_table_node_create(sizeof(C2(HASH_TABLE_NODE_,record_type)), offsetof(C2(HASH_TABLE_NODE_,record_type), item), offsetof(C2(HASH_TABLE_NODE_,record_type), record));

#define CLDS_HASH_TABLE_NODE_DESTROY(record_type, ptr) \
clds_hash_table_node_destroy(ptr);

#define CLDS_HASH_TABLE_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(HASH_TABLE_NODE_,record_type), record)))

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key, CLDS_HASH_TABLE_ITEM*, value, HASH_TABLE_ITEM_CLEANUP_CB, item_cleanup_callback, void*, item_cleanup_callback_context);
MOCKABLE_FUNCTION(, int, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_find, CLDS_HASH_TABLE_HANDLE, clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, key);

// helper APIs for creating/destroying a hash table node
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_ITEM*, clds_hash_table_node_create, size_t, node_size, size_t, item_offset, size_t, record_offset);
MOCKABLE_FUNCTION(, void, clds_hash_table_node_destroy, CLDS_HASH_TABLE_ITEM*, item);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HASH_TABLE_H */
