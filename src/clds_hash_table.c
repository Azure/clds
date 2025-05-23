// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/sync.h"
#include "c_pal/interlocked.h"
#include "c_pal/thandle.h"
#include "c_util/cancellation_token.h"

#include "clds/clds_sorted_list.h"
#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hash_table.h"

/* this is a hash table implementation */

MU_DEFINE_ENUM_STRINGS(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);

typedef struct BUCKET_ARRAY_TAG
{
    struct BUCKET_ARRAY_TAG* volatile_atomic next_bucket;
    volatile_atomic int32_t bucket_count;
    volatile_atomic int32_t item_count;
    volatile_atomic int32_t pending_insert_count;
    CLDS_SORTED_LIST_HANDLE volatile_atomic hash_table[];
} BUCKET_ARRAY;

typedef struct CLDS_HASH_TABLE_TAG
{
    COMPUTE_HASH_FUNC compute_hash;
    KEY_COMPARE_FUNC key_compare_func;
    BUCKET_ARRAY* volatile_atomic first_hash_table;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    volatile_atomic int64_t* sequence_number;
    HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb;
    void* skipped_seq_no_cb_context;

    // Support for locking the list for writes
    volatile_atomic int32_t locked_for_write;
    volatile_atomic int32_t pending_write_operations;
} CLDS_HASH_TABLE;

typedef struct FIND_BY_KEY_VALUE_CONTEXT_TAG
{
    void* key;
    void* value;
    KEY_COMPARE_FUNC key_compare_func;
} FIND_BY_KEY_VALUE_CONTEXT;

static void check_lock_and_begin_write_operation(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    int32_t locked_for_write;
    do
    {
        (void)interlocked_increment(&clds_hash_table->pending_write_operations);
        locked_for_write = interlocked_add(&clds_hash_table->locked_for_write, 0);
        if (locked_for_write != 0)
        {
            (void)interlocked_decrement(&clds_hash_table->pending_write_operations);
            wake_by_address_all(&clds_hash_table->pending_write_operations);

            // Wait for unlock
            (void)wait_on_address(&clds_hash_table->locked_for_write, locked_for_write, UINT32_MAX);
        }
    } while (locked_for_write != 0);
}

static void end_write_operation(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    (void)interlocked_decrement(&clds_hash_table->pending_write_operations);
    wake_by_address_all(&clds_hash_table->pending_write_operations);
}

static void internal_lock_writes(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    /*Codes_SRS_CLDS_HASH_TABLE_42_017: [ clds_hash_table_snapshot shall increment a counter to lock the table for writes. ]*/
    (void)interlocked_increment(&clds_hash_table->locked_for_write);

    /*Codes_SRS_CLDS_HASH_TABLE_42_018: [ clds_hash_table_snapshot shall wait for the ongoing write operations to complete. ]*/
    int32_t pending_writes;
    do
    {
        pending_writes = interlocked_add(&clds_hash_table->pending_write_operations, 0);
        if (pending_writes != 0)
        {
            // Wait for writes
            (void)wait_on_address(&clds_hash_table->pending_write_operations, pending_writes, UINT32_MAX);
        }
    } while (pending_writes != 0);
}

static void internal_unlock_writes(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    /*Codes_SRS_CLDS_HASH_TABLE_42_030: [ clds_hash_table_snapshot shall decrement the counter to unlock the table for writes. ]*/
    (void)interlocked_decrement(&clds_hash_table->locked_for_write);
    wake_by_address_all(&clds_hash_table->locked_for_write);
}

static void* get_item_key_cb(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);
    (void)context;
    return hash_table_item->key;
}

static int key_compare_cb(void* context, void* key1, void* key2)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table = context;
    return clds_hash_table->key_compare_func(key1, key2);
}

static void on_sorted_list_skipped_seq_no(void* context, int64_t skipped_sequence_no)
{
    if (context == NULL)
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_075: [ on_sorted_list_skipped_seq_no called with NULL context shall return. ]*/
        LogError("Invalid arguments: void* context=%p, int64_t skipped_sequence_no=%" PRId64,
            context, skipped_sequence_no);
    }
    else
    {
        CLDS_HASH_TABLE_HANDLE clds_hash_table = context;
        if (clds_hash_table->skipped_seq_no_cb == NULL)
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_113: [ If the sequence number callback passed to clds_hash_table_create was NULL, on_sorted_list_skipped_seq_no shall return. ]*/
        }
        else
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_076: [ on_sorted_list_skipped_seq_no shall call the skipped sequence number callback passed to clds_hash_table_create and pass the skipped_sequence_no as skipped_sequence_no argument. ]*/
            clds_hash_table->skipped_seq_no_cb(clds_hash_table->skipped_seq_no_cb_context, skipped_sequence_no);
        }
    }
}

static BUCKET_ARRAY* get_first_bucket_array(CLDS_HASH_TABLE* clds_hash_table)
{
    // always insert in the first bucket array
    BUCKET_ARRAY* first_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
    int32_t bucket_count = interlocked_add(&first_bucket_array->bucket_count, 0);
    while (interlocked_add(&first_bucket_array->item_count, 0) >= bucket_count)
    {
        // allocate a new bucket array
        BUCKET_ARRAY* new_bucket_array = malloc_flex(sizeof(BUCKET_ARRAY), bucket_count, sizeof(CLDS_SORTED_LIST_HANDLE) * 2);
        if (new_bucket_array == NULL)
        {
            // cannot allocate new bucket, will stick to what we have, but do not fail
            break;
        }
        else
        {
            // insert new bucket
            /* Codes_SRS_CLDS_HASH_TABLE_01_030: [ If the number of items in the list reaches the number of buckets, the number of buckets shall be doubled. ]*/
            bucket_count = bucket_count * 2;
            (void)interlocked_exchange(&new_bucket_array->bucket_count, bucket_count);
            (void)interlocked_exchange(&new_bucket_array->item_count, 0);
            (void)interlocked_exchange(&new_bucket_array->pending_insert_count, 0);

            // initialize buckets
            for (int32_t i = 0; i < bucket_count; i++)
            {
                (void)interlocked_exchange_pointer((void* volatile_atomic*)&new_bucket_array->hash_table[i], NULL);
            }

            (void)interlocked_exchange_pointer((void* volatile_atomic*)&new_bucket_array->next_bucket, first_bucket_array);
            if (interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, new_bucket_array, first_bucket_array) == first_bucket_array)
            {
                first_bucket_array = new_bucket_array;
                break;
            }
            else
            {
                // first bucket array changed, drop ours and use the one that was inserted
                free(new_bucket_array);

                first_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
                bucket_count = interlocked_add(&first_bucket_array->bucket_count, 0);
            }
        }
    }

    return first_bucket_array;
}

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table;

    /* Codes_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
    /* Codes_S_R_S_CLDS_HASH_TABLE_01_072: [ skipped_seq_no_cb shall be allowed to be NULL. ]*/
    /* Codes_S_R_S_CLDS_HASH_TABLE_01_073: [ skipped_seq_no_cb_context shall be allowed to be NULL. ]*/

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_003: [ If compute_hash is NULL, clds_hash_table_create shall fail and return NULL. ]*/
        (compute_hash == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_045: [ If key_compare_func is NULL, clds_hash_table_create shall fail and return NULL. ]*/
        (key_compare_func == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_004: [ If initial_bucket_size is 0, clds_hash_table_create shall fail and return NULL. ]*/
        (initial_bucket_size == 0) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_005: [ If clds_hazard_pointers is NULL, clds_hash_table_create shall fail and return NULL. ]*/
        (clds_hazard_pointers == NULL) ||
        /* Codes_S_R_S_CLDS_HASH_TABLE_01_074: [ If start_sequence_number is NULL, then skipped_seq_no_cb must also be NULL, otherwise clds_sorted_list_create shall fail and return NULL. ]*/
        ((start_sequence_number == NULL) && (skipped_seq_no_cb != NULL))
        )
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, clds_hash_table_create shall fail and return NULL. ]*/
        LogError("Invalid arguments: COMPUTE_HASH_FUNC compute_hash=%p, KEY_COMPARE_FUNC key_compare_func=%p, size_t initial_bucket_size=%zu, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p, volatile_atomic int64_t* start_sequence_number=%p, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb=%p, void* skipped_seq_no_cb_context=%p",
            compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
        clds_hash_table = malloc(sizeof(CLDS_HASH_TABLE));
        if (clds_hash_table == NULL)
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, clds_hash_table_create shall fail and return NULL. ]*/
            LogError("Cannot allocate memory for hash table");
        }
        else
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
            clds_hash_table->first_hash_table = malloc_flex(sizeof(BUCKET_ARRAY), initial_bucket_size, sizeof(CLDS_SORTED_LIST_HANDLE));
            if (clds_hash_table->first_hash_table == NULL)
            {
                LogError("Cannot allocate memory for hash table array. Failure in malloc_flex(sizeof(BUCKET_ARRAY)=%zu, initial_bucket_size=%zu, sizeof(CLDS_SORTED_LIST_HANDLE)=%zu);",
                    sizeof(BUCKET_ARRAY), initial_bucket_size, sizeof(CLDS_SORTED_LIST_HANDLE));
            }
            else
            {
                size_t i;

                // all OK
                clds_hash_table->clds_hazard_pointers = clds_hazard_pointers;
                clds_hash_table->compute_hash = compute_hash;
                clds_hash_table->key_compare_func = key_compare_func;
                clds_hash_table->skipped_seq_no_cb = skipped_seq_no_cb;
                clds_hash_table->skipped_seq_no_cb_context = skipped_seq_no_cb_context;

                (void)interlocked_exchange(&clds_hash_table->pending_write_operations, 0);
                (void)interlocked_exchange(&clds_hash_table->locked_for_write, 0);

                /* Codes_SRS_CLDS_HASH_TABLE_01_057: [ start_sequence_number shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. ]*/
                clds_hash_table->sequence_number = start_sequence_number;

                // set the initial bucket count
                (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table->next_bucket, NULL);
                (void)interlocked_exchange(&clds_hash_table->first_hash_table->bucket_count, (int32_t)initial_bucket_size);
                (void)interlocked_exchange(&clds_hash_table->first_hash_table->item_count, 0);
                (void)interlocked_exchange(&clds_hash_table->first_hash_table->pending_insert_count, 0);

                for (i = 0; i < initial_bucket_size; i++)
                {
                    (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table->hash_table[i], NULL);
                }

                goto all_ok;
            }

            free(clds_hash_table);
        }
    }

    clds_hash_table = NULL;

all_ok:
    return clds_hash_table;
}

static void sorted_list_item_cleanup(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);

    (void)context;
    if (hash_table_item->item_cleanup_callback != NULL)
    {
        hash_table_item->item_cleanup_callback(hash_table_item->item_cleanup_callback_context, (void*)item);
    }
}

void clds_hash_table_destroy(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    if (clds_hash_table == NULL)
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_007: [ If clds_hash_table is NULL, clds_hash_table_destroy shall return. ]*/
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p", clds_hash_table);
    }
    else
    {
        int32_t i;

        BUCKET_ARRAY* bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&bucket_array->next_bucket, NULL, NULL);

            /* Codes_SRS_CLDS_HASH_TABLE_01_006: [ clds_hash_table_destroy shall free all resources associated with the hash table instance. ]*/
            for (i = 0; i < bucket_array->bucket_count; i++)
            {
                if (bucket_array->hash_table[i] != NULL)
                {
                    CLDS_SORTED_LIST_HANDLE linked_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&bucket_array->hash_table[i], NULL, NULL);
                    if (linked_list != NULL)
                    {
                        clds_sorted_list_destroy(linked_list);
                    }
                }
            }

            free(bucket_array);
            bucket_array = next_bucket_array;
        }

        free(clds_hash_table);
    }
}

CLDS_HASH_TABLE_INSERT_RESULT clds_hash_table_insert(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_INSERT_RESULT result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_010: [ If clds_hash_table is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_011: [ If key is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_012: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_062: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, CLDS_HASH_TABLE_ITEM* value=%p, int64_t* sequence_number=%p",
            clds_hash_table, clds_hazard_pointers_thread, key, value, sequence_number);
        result = CLDS_HASH_TABLE_INSERT_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_42_032: [ clds_hash_table_insert shall try the following until it acquires a write lock for the table: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_033: [ clds_hash_table_insert shall increment the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_034: [ If the counter to lock the table for writes is non-zero then: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_035: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_036: [ clds_hash_table_insert shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_hash_table);

        bool restart_needed;
        CLDS_SORTED_LIST_HANDLE bucket_list = NULL;
        HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, value);
        uint64_t hash;
        BUCKET_ARRAY* current_bucket_array;
        int32_t bucket_count;
        uint64_t bucket_index;
        bool found_in_lower_levels = false;

        // find or allocate a new bucket array
        current_bucket_array = get_first_bucket_array(clds_hash_table);
        bucket_count = interlocked_add(&current_bucket_array->bucket_count, 0);

        (void)interlocked_increment(&current_bucket_array->pending_insert_count);

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_038: [ clds_hash_table_insert shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
        hash = clds_hash_table->compute_hash(key);

        found_in_lower_levels = false;

        // check if the key exists in the lower level bucket arrays
        BUCKET_ARRAY* find_bucket_array = current_bucket_array;
        BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&find_bucket_array->next_bucket, NULL, NULL);

        if (next_bucket_array != NULL)
        {
            // wait for all outstanding inserts in the lower levels to complete
            do
            {
                if (interlocked_add(&next_bucket_array->pending_insert_count, 0) == 0)
                {
                    break;
                }
            } while (1);
        }

        find_bucket_array = next_bucket_array;
        while (find_bucket_array != NULL)
        {
            next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&find_bucket_array->next_bucket, NULL, NULL);

            bucket_index = hash % interlocked_add(&find_bucket_array->bucket_count, 0);
            bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&find_bucket_array->hash_table[bucket_index], NULL, NULL);

            if (bucket_list != NULL)
            {
                CLDS_SORTED_LIST_ITEM* sorted_list_item = clds_sorted_list_find_key(bucket_list, clds_hazard_pointers_thread, key);
                if (sorted_list_item != NULL)
                {
                    clds_sorted_list_node_release(sorted_list_item);

                    found_in_lower_levels = true;
                    break;
                }
            }

            find_bucket_array = next_bucket_array;
        }

        if (found_in_lower_levels)
        {
            result = CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS;
        }
        else
        {
            (void)interlocked_increment(&current_bucket_array->item_count);

            // find the bucket
            /* Codes_SRS_CLDS_HASH_TABLE_01_018: [ clds_hash_table_insert shall obtain the bucket index to be used by calling compute_hash and passing to it the key value. ]*/
            bucket_index = hash % bucket_count;

            do
            {
                // do we have a list here or do we create one?
                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    restart_needed = false;
                }
                else
                {
                    // create a list
                    /* Codes_SRS_CLDS_HASH_TABLE_01_019: [ If no sorted list exists at the determined bucket index then a new list shall be created. ]*/
                    /* Codes_SRS_CLDS_HASH_TABLE_01_071: [ When a new list is created, the start sequence number passed to clds_hash_tabel_create shall be passed as the start_sequence_number argument. ]*/
                    bucket_list = clds_sorted_list_create(clds_hash_table->clds_hazard_pointers, get_item_key_cb, clds_hash_table, key_compare_cb, clds_hash_table, clds_hash_table->sequence_number, clds_hash_table->sequence_number == NULL ? NULL : on_sorted_list_skipped_seq_no, clds_hash_table);
                    if (bucket_list == NULL)
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
                        LogError("Cannot allocate list for hash table bucket");
                        restart_needed = false;
                    }
                    else
                    {
                        // now put the list in the bucket
                        if (interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], bucket_list, NULL) != NULL)
                        {
                            // oops, someone else inserted a new list, just bail on our list and restart
                            clds_sorted_list_destroy(bucket_list);
                            restart_needed = true;
                        }
                        else
                        {
                            // set new list
                            restart_needed = false;
                        }
                    }
                }
            } while (restart_needed);

            if (bucket_list == NULL)
            {
                (void)interlocked_decrement(&current_bucket_array->item_count);

                LogError("Cannot acquire bucket list");
                result = CLDS_HASH_TABLE_INSERT_ERROR;
            }
            else
            {
                CLDS_SORTED_LIST_INSERT_RESULT list_insert_result;

                /* Codes_SRS_CLDS_HASH_TABLE_01_020: [ A new sorted list item shall be created by calling clds_sorted_list_node_create. ]*/
                hash_table_item->key = key;

                /* Codes_SRS_CLDS_HASH_TABLE_01_021: [ The new sorted list node shall be inserted in the sorted list at the identified bucket by calling clds_sorted_list_insert. ]*/
                /* Codes_SRS_CLDS_HASH_TABLE_01_059: [ For each insert the order of the operation shall be computed by passing sequence_number to clds_sorted_list_insert. ]*/
                list_insert_result = clds_sorted_list_insert(bucket_list, clds_hazard_pointers_thread, (void*)value, sequence_number);

                if (list_insert_result == CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS)
                {
                    (void)interlocked_decrement(&current_bucket_array->item_count);

                    /* Codes_SRS_CLDS_HASH_TABLE_01_046: [ If the key already exists in the hash table, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ALREADY_EXISTS. ]*/
                    result = CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS;
                }
                else if (list_insert_result != CLDS_SORTED_LIST_INSERT_OK)
                {
                    (void)interlocked_decrement(&current_bucket_array->item_count);

                    /* Codes_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
                    LogError("Cannot insert hash table item into list");
                    result = CLDS_HASH_TABLE_INSERT_ERROR;
                }
                else
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_009: [ On success clds_hash_table_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
                    result = CLDS_HASH_TABLE_INSERT_OK;
                }
            }
        }

        (void)interlocked_decrement(&current_bucket_array->pending_insert_count);

        /* Codes_SRS_CLDS_HASH_TABLE_42_063: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_hash_table);
    }
    return result;
}

static bool find_by_key_value(void* item_compare_context, CLDS_SORTED_LIST_ITEM* item)
{
    bool result;
    FIND_BY_KEY_VALUE_CONTEXT* find_by_key_value_context = item_compare_context;
    HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);

    if ((item != find_by_key_value_context->value) ||
        (find_by_key_value_context->key_compare_func(hash_table_item->key, find_by_key_value_context->key) != 0))
    {
        result = false;
    }
    else
    {
        result = true;
    }

    return result;
}

CLDS_HASH_TABLE_DELETE_RESULT clds_hash_table_delete(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_DELETE_RESULT result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_015: [ If clds_hash_table is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_017: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_016: [ If key is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_066: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, int64_t* sequence_number=%p",
            clds_hash_table, key, clds_hazard_pointers_thread, sequence_number);
        result = CLDS_HASH_TABLE_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_42_037: [ clds_hash_table_delete shall try the following until it acquires a write lock for the table: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_038: [ clds_hash_table_delete shall increment the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_039: [ If the counter to lock the table for writes is non-zero then: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_040: [ clds_hash_table_delete shall decrement the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_041: [ clds_hash_table_delete shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_hash_table);

        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_039: [ clds_hash_table_delete shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;

        // always delete starting with the first bucket array
        /* Codes_SRS_CLDS_HASH_TABLE_01_101: [ Otherwise, key shall be looked up in each of the arrays of buckets starting with the first. ]*/
        current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

            if (interlocked_add(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % interlocked_add(&current_bucket_array->bucket_count, 0);

                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
                    result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_DELETE_RESULT list_delete_result;

                    /* Codes_SRS_CLDS_HASH_TABLE_01_063: [ For each delete the order of the operation shall be computed by passing sequence_number to clds_sorted_list_delete_key. ]*/
                    list_delete_result = clds_sorted_list_delete_key(bucket_list, clds_hazard_pointers_thread, key, sequence_number);
                    if (list_delete_result == CLDS_SORTED_LIST_DELETE_NOT_FOUND)
                    {
                        // not found
                        /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
                    }
                    else if (list_delete_result == CLDS_SORTED_LIST_DELETE_OK)
                    {
                        (void)interlocked_decrement(&current_bucket_array->item_count);

                        /* Codes_SRS_CLDS_HASH_TABLE_01_014: [ On success clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_OK. ]*/
                        result = CLDS_HASH_TABLE_DELETE_OK;
                        break;
                    }
                    else
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_024: [ If a bucket is identified and the delete of the item from the underlying list fails, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
                        result = CLDS_HASH_TABLE_DELETE_ERROR;
                        break;
                    }
                }
            }

            /* Codes_SRS_CLDS_HASH_TABLE_01_025: [ If the element to be deleted is not found in an array of buckets, then it shall be looked up in the next available array of buckets. ] */
            current_bucket_array = next_bucket_array;
        }

        /* Codes_SRS_CLDS_HASH_TABLE_42_042: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_hash_table);
    }

    return result;
}

CLDS_HASH_TABLE_DELETE_RESULT clds_hash_table_delete_key_value(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_DELETE_RESULT result;

    if (
        /*Codes_SRS_CLDS_HASH_TABLE_42_003: [ If clds_hash_table is NULL, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /*Codes_SRS_CLDS_HASH_TABLE_42_005: [ If key is NULL, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (key == NULL) ||
        /*Codes_SRS_CLDS_HASH_TABLE_42_006: [ If value is NULL, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (value == NULL) ||
        /*Codes_SRS_CLDS_HASH_TABLE_42_004: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /*Codes_SRS_CLDS_HASH_TABLE_42_012: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, CLDS_HASH_TABLE_ITEM* value=%p, int64_t* sequence_number=%p",
            clds_hash_table, key, value, clds_hazard_pointers_thread, sequence_number);
        result = CLDS_HASH_TABLE_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_42_043: [ clds_hash_table_delete_key_value shall try the following until it acquires a write lock for the table: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_044: [ clds_hash_table_delete_key_value shall increment the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_045: [ If the counter to lock the table for writes is non-zero then: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_046: [ clds_hash_table_delete_key_value shall decrement the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_047: [ clds_hash_table_delete_key_value shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_hash_table);

        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        // compute the hash
        /*Codes_SRS_CLDS_HASH_TABLE_42_001: [ clds_hash_table_delete_key_value shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;

        // always insert in the first bucket array
        /*Codes_SRS_CLDS_HASH_TABLE_42_007: [ Otherwise, key shall be looked up in each of the arrays of buckets starting with the first. ]*/
        current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

            if (interlocked_add(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % interlocked_add(&current_bucket_array->bucket_count, 0);

                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    /*Codes_SRS_CLDS_HASH_TABLE_42_008: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete_key_value shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
                    result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_DELETE_RESULT list_delete_result;

                    /*Codes_SRS_CLDS_HASH_TABLE_42_011: [ For each delete the order of the operation shall be computed by passing sequence_number to clds_sorted_list_delete_item. ]*/
                    list_delete_result = clds_sorted_list_delete_item(bucket_list, clds_hazard_pointers_thread, (void*)value, sequence_number);
                    if (list_delete_result == CLDS_SORTED_LIST_DELETE_NOT_FOUND)
                    {
                        // not found
                    }
                    else if (list_delete_result == CLDS_SORTED_LIST_DELETE_OK)
                    {
                        (void)interlocked_decrement(&current_bucket_array->item_count);

                        /*Codes_SRS_CLDS_HASH_TABLE_42_002: [ On success clds_hash_table_delete_key_value shall return CLDS_HASH_TABLE_DELETE_OK. ]*/
                        result = CLDS_HASH_TABLE_DELETE_OK;
                        break;
                    }
                    else
                    {
                        /*Codes_SRS_CLDS_HASH_TABLE_42_009: [ If a bucket is identified and the delete of the item from the underlying list fails, clds_hash_table_delete_key_value shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
                        result = CLDS_HASH_TABLE_DELETE_ERROR;
                        break;
                    }
                }
            }

            /*Codes_SRS_CLDS_HASH_TABLE_42_010: [ If the element to be deleted is not found in an array of buckets, then clds_hash_table_delete_key_value shall look in the next available array of buckets. ]*/
            current_bucket_array = next_bucket_array;
        }

        /* Codes_SRS_CLDS_HASH_TABLE_42_048: [ clds_hash_table_delete_key_value shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_hash_table);
    }

    return result;
}

CLDS_HASH_TABLE_REMOVE_RESULT clds_hash_table_remove(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM** item, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_REMOVE_RESULT result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_050: [ If clds_hash_table is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_052: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_051: [ If key is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_056: [ If item is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_070: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, CLDS_HASH_TABLE_ITEM** item=%p, int64_t* sequence_number=%p",
            clds_hash_table, key, clds_hazard_pointers_thread, item, sequence_number);
        result = CLDS_HASH_TABLE_REMOVE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_42_049: [ clds_hash_table_remove shall try the following until it acquires a write lock for the table: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_050: [ clds_hash_table_remove shall increment the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_051: [ If the counter to lock the table for writes is non-zero then: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_052: [ clds_hash_table_remove shall decrement the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_053: [ clds_hash_table_remove shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_hash_table);

        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        /* Codes_SRS_CLDS_HASH_TABLE_01_047: [ clds_hash_table_remove shall remove a key from the hash table and return a pointer to the item to the user. ]*/

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_048: [ clds_hash_table_remove shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_REMOVE_NOT_FOUND;

        // always insert in the first bucket array
        current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

            if (interlocked_add(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % interlocked_add(&current_bucket_array->bucket_count, 0);

                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_053: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_NOT_FOUND. ]*/
                    result = CLDS_HASH_TABLE_REMOVE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_REMOVE_RESULT list_remove_result;
                    /* Codes_SRS_CLDS_HASH_TABLE_01_067: [ For each remove the order of the operation shall be computed by passing sequence_number to clds_sorted_list_remove_key. ]*/
                    list_remove_result = clds_sorted_list_remove_key(bucket_list, clds_hazard_pointers_thread, key, (void*)item, sequence_number);
                    if (list_remove_result == CLDS_SORTED_LIST_REMOVE_NOT_FOUND)
                    {
                        // not found
                    }
                    else if (list_remove_result == CLDS_SORTED_LIST_REMOVE_OK)
                    {
                        (void)interlocked_decrement(&current_bucket_array->item_count);

                        /* Codes_SRS_CLDS_HASH_TABLE_01_049: [ On success clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_OK. ]*/
                        result = CLDS_HASH_TABLE_REMOVE_OK;
                        break;
                    }
                    else
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_054: [ If a bucket is identified and the delete of the item from the underlying list fails, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
                        result = CLDS_HASH_TABLE_REMOVE_ERROR;
                        break;
                    }
                }
            }

            /* Codes_SRS_CLDS_HASH_TABLE_01_055: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
            current_bucket_array = next_bucket_array;
        }

        /* Codes_SRS_CLDS_HASH_TABLE_42_054: [ clds_hash_table_remove shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_hash_table);
    }

    return result;
}

CLDS_HASH_TABLE_SET_VALUE_RESULT clds_hash_table_set_value(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* new_item, CONDITION_CHECK_CB condition_check_func, void* condition_check_context, CLDS_HASH_TABLE_ITEM** old_item, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_079: [ If clds_hash_table is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_080: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_081: [ If key is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_082: [ If new_item is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        (new_item == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_083: [ If old_item is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        (old_item == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_084: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, CLDS_HASH_TABLE_ITEM* new_item=%p, CONDITION_CHECK_CB condition_check_func=%p, void* condition_check_context=%p, CLDS_HASH_TABLE_ITEM** old_item=%p, int64_t* sequence_number=%p",
            clds_hash_table, key, clds_hazard_pointers_thread, new_item, condition_check_func, condition_check_context, old_item, sequence_number);
        result = CLDS_HASH_TABLE_SET_VALUE_ERROR;
    }
    else
    {
        uint64_t bucket_index;
        CLDS_SORTED_LIST_HANDLE bucket_list;

        /* Codes_SRS_CLDS_HASH_TABLE_42_055: [ clds_hash_table_set_value shall try the following until it acquires a write lock for the table: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_056: [ clds_hash_table_set_value shall increment the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_057: [ If the counter to lock the table for writes is non-zero then: ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_058: [ clds_hash_table_set_value shall decrement the count of pending write operations. ]*/
        /* Codes_SRS_CLDS_HASH_TABLE_42_059: [ clds_hash_table_set_value shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_hash_table);

        // compute the hash
        uint64_t hash = clds_hash_table->compute_hash(key);

        // find or allocate a new bucket array
        BUCKET_ARRAY* first_bucket_array = get_first_bucket_array(clds_hash_table);

        // increment pending inserts count
        (void)interlocked_increment(&first_bucket_array->pending_insert_count);

        BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&first_bucket_array->next_bucket, NULL, NULL);
        if (next_bucket_array != NULL)
        {
            // wait for all outstanding inserts in the lower levels to complete
            do
            {
                if (interlocked_add(&next_bucket_array->pending_insert_count, 0) == 0)
                {
                    break;
                }
            } while (1);
        }

        /* Codes_SRS_CLDS_HASH_TABLE_01_085: [ clds_hash_table_set_value shall go through all non top level bucket arrays and: ]*/
        bool set_value_in_top_level = true;
        result = CLDS_HASH_TABLE_SET_VALUE_ERROR;

        BUCKET_ARRAY* find_bucket_array = next_bucket_array;
        while (find_bucket_array != NULL)
        {
            next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&find_bucket_array->next_bucket, NULL, NULL);

            bucket_index = hash % interlocked_add(&find_bucket_array->bucket_count, 0);
            bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&find_bucket_array->hash_table[bucket_index], NULL, NULL);

            if (bucket_list != NULL)
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_108: [ If there is a sorted list in the bucket identified by the hash of the key, clds_hash_table_set_value shall find the key in the list. ]*/
                CLDS_SORTED_LIST_ITEM* sorted_list_item = clds_sorted_list_find_key(bucket_list, clds_hazard_pointers_thread, key);
                if (sorted_list_item != NULL)
                {
                    clds_sorted_list_node_release(sorted_list_item);

                    HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, new_item);
                    hash_table_item->key = key;

                    /* Codes_SRS_CLDS_HASH_TABLE_01_110: [ If the key is found, clds_hash_table_set_value shall call clds_sorted_list_set_value with the key, new_item, condition_check_func, condition_check_context and old_item and only_if_exists set to true. ]*/
                    CLDS_SORTED_LIST_SET_VALUE_RESULT sorted_list_set_value_result = clds_sorted_list_set_value(bucket_list, clds_hazard_pointers_thread, key, (void*)new_item, condition_check_func, condition_check_context, (void*)old_item, sequence_number, true);
                    switch (sorted_list_set_value_result)
                    {
                    default:
                    case CLDS_SORTED_LIST_SET_VALUE_ERROR:
                        /* Codes_SRS_CLDS_HASH_TABLE_01_111: [ If clds_sorted_list_set_value fails, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
                        LogError("Cannot set key in sorted list: failed with %" PRI_MU_ENUM "", MU_ENUM_VALUE(CLDS_SORTED_LIST_SET_VALUE_RESULT, sorted_list_set_value_result));
                        set_value_in_top_level = false;
                        break;

                    case CLDS_SORTED_LIST_SET_VALUE_OK:
                        /* Codes_SRS_CLDS_HASH_TABLE_01_112: [ If clds_sorted_list_set_value succeeds, clds_hash_table_set_value shall return CLDS_HASH_TABLE_SET_VALUE_OK. ]*/
                        result = CLDS_HASH_TABLE_SET_VALUE_OK;
                        set_value_in_top_level = false;
                        break;
                    
                    case CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET:
                        /* Codes_SRS_CLDS_HASH_TABLE_04_001: [ If clds_sorted_list_set_value returns CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET. ]*/
                        result = CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET;
                        set_value_in_top_level = false;
                        break;

                    case CLDS_SORTED_LIST_SET_VALUE_NOT_FOUND:
                        /* Codes_SRS_CLDS_HASH_TABLE_01_109: [ If the key is not found, clds_hash_table_set_value shall advance to the next level of buckets. ]*/
                        break;
                    }
                }
            }
            else
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_107: [ If there is no sorted list in the bucket identified by the hash of the key, clds_hash_table_set_value shall advance to the next level of buckets. ]*/
            }

            find_bucket_array = next_bucket_array;
        }

        if (set_value_in_top_level)
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_102: [ If the key is not found in any of the non top level buckets arrays, clds_hash_table_set_value: ]*/
            BUCKET_ARRAY* current_bucket_array = first_bucket_array;

            // look for the item in this bucket array
            // find the bucket
            bucket_index = hash % interlocked_add(&current_bucket_array->bucket_count, 0);
            bool restart_needed = false;

            do
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_103: [ clds_hash_table_set_value shall obtain the sorted list at the bucket corresponding to the hash of the key. ]*/
                // do we have a list here or do we create one?
                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*) &current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    restart_needed = false;
                }
                else
                {
                    // create a list
                    /* Codes_SRS_CLDS_HASH_TABLE_01_104: [  If no list exists at the designated bucket, one shall be created. ]*/
                    bucket_list = clds_sorted_list_create(clds_hash_table->clds_hazard_pointers, get_item_key_cb, clds_hash_table, key_compare_cb, clds_hash_table, clds_hash_table->sequence_number, clds_hash_table->sequence_number == NULL ? NULL : on_sorted_list_skipped_seq_no, clds_hash_table);
                    if (bucket_list == NULL)
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_106: [ If any error occurs, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
                        LogError("Cannot allocate list for hash table bucket");
                        restart_needed = false;
                    }
                    else
                    {
                        // now put the list in the bucket
                        if (interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], bucket_list, NULL) != NULL)
                        {
                            // oops, someone else inserted a new list, just bail on our list and restart
                            clds_sorted_list_destroy(bucket_list);
                            restart_needed = true;
                        }
                        else
                        {
                            // set new list
                            restart_needed = false;
                        }
                    }
                }
            } while (restart_needed);

            if (bucket_list == NULL)
            {
                LogError("Cannot acquire bucket list");
                result = CLDS_HASH_TABLE_SET_VALUE_ERROR;
            }
            else
            {
                HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, new_item);

                hash_table_item->key = key;

                /* Codes_SRS_CLDS_HASH_TABLE_01_105: [ clds_hash_table_set_value shall call clds_hash_table_set_value on the top level bucket array, passing key, new_item, condition_check_func, condition_check_context, old_item and only_if_exists set to false. ]*/
                CLDS_SORTED_LIST_SET_VALUE_RESULT sorted_list_set_value = clds_sorted_list_set_value(bucket_list, clds_hazard_pointers_thread, key, (void*)new_item, condition_check_func, condition_check_context, (void*)old_item, sequence_number, false);
                if (sorted_list_set_value == CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_04_002: [ If clds_sorted_list_set_value returns CLDS_SORTED_LIST_SET_VALUE_CONDITION_NOT_MET, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET. ]*/
                    LogError("Condition not met during set value - %" PRI_MU_ENUM "", MU_ENUM_VALUE(CLDS_SORTED_LIST_SET_VALUE_RESULT, sorted_list_set_value));
                    result = CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET;
                }
                else if (sorted_list_set_value != CLDS_SORTED_LIST_SET_VALUE_OK)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_100: [ If clds_sorted_list_set_value returns any other value, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
                    LogError("Cannot set key in sorted list - %" PRI_MU_ENUM "", MU_ENUM_VALUE(CLDS_SORTED_LIST_SET_VALUE_RESULT, sorted_list_set_value));
                    result = CLDS_HASH_TABLE_SET_VALUE_ERROR;
                }
                else
                {
                    if (*old_item == NULL)
                    {
                        (void)interlocked_increment(&first_bucket_array->item_count);
                    }

                    /* Codes_SRS_CLDS_HASH_TABLE_01_099: [ If clds_sorted_list_set_value returns CLDS_SORTED_LIST_SET_VALUE_OK, clds_hash_table_set_value shall succeed and return CLDS_HASH_TABLE_SET_VALUE_OK. ]*/
                    result = CLDS_HASH_TABLE_SET_VALUE_OK;
                }
            }
        }

        (void)interlocked_decrement(&first_bucket_array->pending_insert_count);

        /* Codes_SRS_CLDS_HASH_TABLE_42_060: [ clds_hash_table_set_value shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_hash_table);
    }

    return result;
}

CLDS_HASH_TABLE_ITEM* clds_hash_table_find(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_HASH_TABLE_ITEM* result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_01_035: [ If clds_hash_table is NULL, clds_hash_table_find shall fail and return NULL. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_036: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_find shall fail and return NULL. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_037: [ If key is NULL, clds_hash_table_find shall fail and return NULL. ]*/
        (key == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p",
            clds_hash_table, clds_hazard_pointers_thread, key);
        result = NULL;
    }
    else
    {
        CLDS_SORTED_LIST_HANDLE bucket_list;

        result = NULL;

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_040: [ clds_hash_table_find shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        /* Codes_SRS_CLDS_HASH_TABLE_01_041: [ clds_hash_table_find shall look up the key in the biggest array of buckets. ]*/
        BUCKET_ARRAY* current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

            if (interlocked_add(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                /* Codes_SRS_CLDS_HASH_TABLE_01_044: [ Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling clds_sorted_list_find. ]*/
                uint64_t bucket_index = hash % interlocked_add(&current_bucket_array->bucket_count, 0);

                bucket_list = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_034: [ clds_hash_table_find shall find the key identified by key in the hash table and on success return the item corresponding to it. ]*/
                    result = (void*)clds_sorted_list_find_key(bucket_list, clds_hazard_pointers_thread, key);
                    if (result == NULL)
                    {
                        // go to the next level of buckets
                    }
                    else
                    {
                        // found
                        break;
                    }
                }
            }

            /* Codes_SRS_CLDS_HASH_TABLE_01_042: [ If the key is not found in the biggest array of buckets, the next bucket arrays shall be looked up. ]*/
            current_bucket_array = next_bucket_array;
        }

        if (current_bucket_array == NULL)
        {
            /* not found */
            /* Codes_SRS_CLDS_HASH_TABLE_01_043: [ If the key is not found at all, clds_hash_table_find shall return NULL. ]*/
            result = NULL;
        }
        else
        {
            // all OK
        }
    }

    return result;
}

CLDS_HASH_TABLE_SNAPSHOT_RESULT clds_hash_table_snapshot(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_HASH_TABLE_ITEM*** items, uint64_t* item_count, THANDLE(CANCELLATION_TOKEN) cancellation_token)
{
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result;

    if (
        /* Codes_SRS_CLDS_HASH_TABLE_42_013: [ If clds_hash_table is NULL then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
        (clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_42_014: [ If clds_hazard_pointers_thread is NULL then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_42_015: [ If items is NULL then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
        (items == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_42_016: [ If item_count is NULL then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
        (item_count == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_HANDLE clds_hash_table=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, CLDS_HASH_TABLE_ITEM*** items=%p, uint64_t* item_count=%p, THANDLE(CANCELLATION_TOKEN) cancellation_token=%p",
            clds_hash_table, clds_hazard_pointers_thread, items, item_count, cancellation_token);
        result = CLDS_HASH_TABLE_SNAPSHOT_ERROR;
    }
    else
    {
        internal_lock_writes(clds_hash_table);

        uint64_t temp_item_count = 0;

        /* Codes_SRS_CLDS_HASH_TABLE_01_114: [ clds_hash_table_snapshot shall determine all the items in the hash table by summing up the item count for all bucket arrays in all levels. ]*/
        BUCKET_ARRAY* current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

            temp_item_count += interlocked_add(&current_bucket_array->item_count, 0);

            current_bucket_array = next_bucket_array;
        }

        if (temp_item_count == 0)
        {
            /* Codes_SRS_CLDS_HASH_TABLE_42_064: [ If there are no items then clds_hash_table_snapshot shall set items to NULL and item_count to 0 and return CLDS_HASH_TABLE_SNAPSHOT_OK. ]*/
            *items = NULL;
            *item_count = 0;
            result = CLDS_HASH_TABLE_SNAPSHOT_OK;
        }
        else
        {
            /* Codes_SRS_CLDS_HASH_TABLE_42_023: [ clds_hash_table_snapshot shall allocate an array of CLDS_HASH_TABLE_ITEM* ]*/
            CLDS_SORTED_LIST_ITEM** items_to_return = malloc_2((size_t)temp_item_count, sizeof(CLDS_SORTED_LIST_ITEM*));

            if (items_to_return == NULL)
            {
                /* Codes_SRS_CLDS_HASH_TABLE_42_061: [ If there are any other failures then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
                LogError("malloc_2((size_t)temp_item_count=%zu, sizeof(CLDS_SORTED_LIST_ITEM*)=%zu) failed for the items to return",
                    (size_t)temp_item_count, sizeof(CLDS_SORTED_LIST_ITEM*));
                result = CLDS_HASH_TABLE_SNAPSHOT_ERROR;
            }
            else
            {
                uint64_t result_index = 0;
                bool is_cancelled = false;

                /* Codes_SRS_CLDS_HASH_TABLE_42_024: [ For each bucket in the array: ]*/
                current_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hash_table->first_hash_table, NULL, NULL);
                while (current_bucket_array != NULL)
                {
                    BUCKET_ARRAY* next_bucket_array = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_bucket_array->next_bucket, NULL, NULL);

                    if (interlocked_add(&current_bucket_array->item_count, 0) != 0)
                    {
                        int32_t bucket_count = interlocked_add(&current_bucket_array->bucket_count, 0);
                        int32_t i;

                        for (i = 0; i < bucket_count; i++)
                        {
                            if ((current_bucket_array->hash_table[i] != NULL) && (temp_item_count > 0))
                            {
                                uint64_t retrieved_item_count;

                                if (
                                    /* Codes_SRS_CLDS_HASH_TABLE_01_115: [ If cancellation_token is non-NULL and cancellation_token_is_cancelled returns true for cancellation_token, clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ABANDONED. ]*/
                                    (cancellation_token != NULL) &&
                                    (cancellation_token_is_canceled(cancellation_token))
                                    )
                                {
                                    LogVerbose("snapshot cancelled");
                                    is_cancelled = true;
                                    break;
                                }

                                /* Codes_SRS_CLDS_HASH_TABLE_42_026: [ clds_hash_table_snapshot shall call clds_sorted_list_get_all with the next portion of the allocated array and false as required_locked_list. ]*/
                                CLDS_SORTED_LIST_GET_ALL_RESULT get_all_result = clds_sorted_list_get_all(current_bucket_array->hash_table[i], clds_hazard_pointers_thread, temp_item_count, items_to_return + result_index, &retrieved_item_count, false);
                                if (get_all_result != CLDS_SORTED_LIST_GET_ALL_OK)
                                {
                                    /* Codes_SRS_CLDS_HASH_TABLE_42_061: [ If there are any other failures then clds_hash_table_snapshot shall fail and return CLDS_HASH_TABLE_SNAPSHOT_ERROR. ]*/
                                    LogError("clds_sorted_list_get_all failed with %" PRI_MU_ENUM, MU_ENUM_VALUE(CLDS_SORTED_LIST_GET_ALL_RESULT, get_all_result));
                                    break;
                                }
                                else
                                {
                                    result_index += retrieved_item_count;
                                    temp_item_count -= retrieved_item_count;
                                }
                            }
                        }

                        if (i < bucket_count)
                        {
                            break;
                        }
                    }

                    current_bucket_array = next_bucket_array;
                }

                if (current_bucket_array != NULL)
                {
                    if (is_cancelled)
                    {
                        result = CLDS_HASH_TABLE_SNAPSHOT_ABANDONED;
                    }
                    else
                    {
                        result = CLDS_HASH_TABLE_SNAPSHOT_ERROR;
                    }

                    for (uint64_t i = 0; i < result_index; i++)
                    {
                        clds_sorted_list_node_release(items_to_return[i]);
                    }
                    free(items_to_return);
                }
                else
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_42_028: [ clds_hash_table_snapshot shall store the allocated array of items in items. ]*/
                    *items = (CLDS_HASH_TABLE_ITEM**)items_to_return;
                    items_to_return = NULL;

                    /* Codes_SRS_CLDS_HASH_TABLE_42_029: [ clds_hash_table_snapshot shall store the count of items in item_count. ]*/
                    *item_count = result_index;

                    /* Codes_SRS_CLDS_HASH_TABLE_42_031: [ clds_hash_table_snapshot shall succeed and return CLDS_HASH_TABLE_SNAPSHOT_OK. ]*/
                    result = CLDS_HASH_TABLE_SNAPSHOT_OK;
                }
            }
        }

        internal_unlock_writes(clds_hash_table);
    }

    return result;
}

CLDS_HASH_TABLE_ITEM* clds_hash_table_node_create(size_t node_size, HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    void* result = malloc(node_size);

    if (result == NULL)
    {
        LogError("malloc(node_size=%zu) failed", node_size);
    }
    else
    {
        CLDS_HASH_TABLE_ITEM* item = result;
        HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);
        hash_table_item->item_cleanup_callback = item_cleanup_callback;
        hash_table_item->item_cleanup_callback_context = item_cleanup_callback_context;
        item->item.item_cleanup_callback = sorted_list_item_cleanup;
        item->item.item_cleanup_callback_context = (void*)item;
        (void)interlocked_exchange(&item->item.ref_count, 1);
    }

    return result;
}

int clds_hash_table_node_inc_ref(CLDS_HASH_TABLE_ITEM* item)
{
    int result;

    if (item == NULL)
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_ITEM* item=%p", item);
        result = MU_FAILURE;
    }
    else
    {
        if (clds_sorted_list_node_inc_ref((void*)item) != 0)
        {
            LogError("clds_sorted_list_node_inc_ref failed");
            result = MU_FAILURE;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

void clds_hash_table_node_release(CLDS_HASH_TABLE_ITEM* item)
{
    if (item == NULL)
    {
        LogError("Invalid arguments: CLDS_HASH_TABLE_ITEM* item=%p", item);
    }
    else
    {
        clds_sorted_list_node_release((void*)item);
    }
}
