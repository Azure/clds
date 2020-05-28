// Copyright (c) Microsoft. All rights reserved.

#ifndef REAL_CLDS_HASH_TABLE_H
#define REAL_CLDS_HASH_TABLE_H

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "clds/clds_hash_table.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        clds_hash_table_create, \
        clds_hash_table_destroy, \
        clds_hash_table_insert, \
        clds_hash_table_delete, \
        clds_hash_table_delete_key_value, \
        clds_hash_table_remove, \
        clds_hash_table_set_value, \
        clds_hash_table_find, \
        clds_hash_table_node_create, \
        clds_hash_table_node_inc_ref, \
        clds_hash_table_node_release, \
        clds_hash_table_snapshot \
    )

#ifdef __cplusplus
extern "C" {
#endif

CLDS_HASH_TABLE_HANDLE real_clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context);
void real_clds_hash_table_destroy(CLDS_HASH_TABLE_HANDLE clds_hash_table);
CLDS_HASH_TABLE_INSERT_RESULT real_clds_hash_table_insert(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t* sequence_number);
CLDS_HASH_TABLE_DELETE_RESULT real_clds_hash_table_delete(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, int64_t* sequence_number);
CLDS_HASH_TABLE_DELETE_RESULT real_clds_hash_table_delete_key_value(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t* sequence_number);
CLDS_HASH_TABLE_REMOVE_RESULT real_clds_hash_table_remove(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM** item, int64_t* sequence_number);
CLDS_HASH_TABLE_ITEM* real_clds_hash_table_find(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key);
CLDS_HASH_TABLE_SET_VALUE_RESULT real_clds_hash_table_set_value(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, const void* key, CLDS_HASH_TABLE_ITEM* new_item, CLDS_HASH_TABLE_ITEM** old_item, int64_t* sequence_number);
CLDS_HASH_TABLE_SNAPSHOT_RESULT real_clds_hash_table_snapshot(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_HASH_TABLE_ITEM*** items, uint64_t* item_count);

// helper APIs for creating/destroying a hash table node
CLDS_HASH_TABLE_ITEM* real_clds_hash_table_node_create(size_t node_size, HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context);
int real_clds_hash_table_node_inc_ref(CLDS_HASH_TABLE_ITEM* item);
void real_clds_hash_table_node_release(CLDS_HASH_TABLE_ITEM* item);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_HASH_TABLE_H
