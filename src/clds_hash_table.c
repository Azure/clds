// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifdef _MSC_VER
#pragma warning(disable: 4200)
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "windows.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hash_table.h"
#include "clds/clds_atomics.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_hazard_pointers.h"

/* this is a hash table implementation */

typedef struct BUCKET_ARRAY_TAG
{
    volatile struct BUCKET_ARRAY_TAG* next_bucket;
    volatile LONG bucket_count;
    volatile LONG item_count;
    CLDS_SORTED_LIST_HANDLE hash_table[];
} BUCKET_ARRAY;

typedef struct CLDS_HASH_TABLE_TAG
{
    COMPUTE_HASH_FUNC compute_hash;
    KEY_COMPARE_FUNC key_compare_func;
    volatile BUCKET_ARRAY* first_hash_table;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    volatile LONG64* sequence_number;
} CLDS_HASH_TABLE;

typedef struct FIND_BY_KEY_VALUE_CONTEXT_TAG
{
    void* key;
    void* value;
    KEY_COMPARE_FUNC key_compare_func;
} FIND_BY_KEY_VALUE_CONTEXT;

static void* get_item_key_cb(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);
    (void)context;
    return hash_table_item->key;
}

static int key_compare_cb(void* context, void* key1, void* key2)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table = (CLDS_HASH_TABLE_HANDLE)context;
    return clds_hash_table->key_compare_func(key1, key2);
}

static void sorted_list_skipped_seq_no(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile int64_t* start_sequence_number)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table;

    /* Codes_SRS_CLDS_HASH_TABLE_01_058: [ `start_sequence_number` shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/

    /* Codes_SRS_CLDS_HASH_TABLE_01_003: [ If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
    if ((compute_hash == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_045: [ If `key_compare_func` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
        (key_compare_func == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_004: [ If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. ]*/
        (initial_bucket_size == 0) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_005: [ If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
        (clds_hazard_pointers == NULL))
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, `clds_hash_table_create` shall fail and return NULL. ]*/
        LogError("Invalid arguments: compute_hash = %p, key_compare_func = %p, initial_bucket_size = %zu, clds_hazard_pointers = %p, start_sequence_number = %p",
            compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number);
    }
    else
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_001: [ `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
        clds_hash_table = (CLDS_HASH_TABLE_HANDLE)malloc(sizeof(CLDS_HASH_TABLE));
        if (clds_hash_table == NULL)
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, `clds_hash_table_create` shall fail and return NULL. ]*/
            LogError("Cannot allocate memory for hash table");
        }
        else
        {
            clds_hash_table->first_hash_table = malloc(sizeof(BUCKET_ARRAY) + (sizeof(CLDS_SORTED_LIST_HANDLE) * initial_bucket_size));
            if (clds_hash_table->first_hash_table == NULL)
            {
                LogError("Cannot allocate memory for hash table array");
            }
            else
            {
                size_t i;

                // all OK
                clds_hash_table->clds_hazard_pointers = clds_hazard_pointers;
                clds_hash_table->compute_hash = compute_hash;
                clds_hash_table->key_compare_func = key_compare_func;

                /* Codes_SRS_CLDS_HASH_TABLE_01_057: [ `start_sequence_number` shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. ]*/
                clds_hash_table->sequence_number = start_sequence_number;

                // set the initial bucket count
                (void)InterlockedExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table->next_bucket, NULL);
                (void)InterlockedExchange(&clds_hash_table->first_hash_table->bucket_count, (LONG)initial_bucket_size);
                (void)InterlockedExchange(&clds_hash_table->first_hash_table->item_count, 0);

                for (i = 0; i < initial_bucket_size; i++)
                {
                    (void)InterlockedExchangePointer(&clds_hash_table->first_hash_table->hash_table[i], NULL);
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
        /* Codes_SRS_CLDS_HASH_TABLE_01_007: [ If `clds_hash_table` is NULL, `clds_hash_table_destroy` shall return. ]*/
        LogError("NULL clds_hash_table");
    }
    else
    {
        LONG i;

        BUCKET_ARRAY* bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&bucket_array->next_bucket, NULL, NULL);

            /* Codes_SRS_CLDS_HASH_TABLE_01_006: [ `clds_hash_table_destroy` shall free all resources associated with the hash table instance. ]*/
            for (i = 0; i < bucket_array->bucket_count; i++)
            {
                if (bucket_array->hash_table[i] != NULL)
                {
                    CLDS_SORTED_LIST_HANDLE linked_list = InterlockedCompareExchangePointer(&bucket_array->hash_table[i], NULL, NULL);
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

    /* Codes_SRS_CLDS_HASH_TABLE_01_010: [ If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
    if ((clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_011: [ If `key` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_012: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_062: [ If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL)))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p, sequence_number = %p",
            clds_hash_table, key, sequence_number);
        result = CLDS_HASH_TABLE_INSERT_ERROR;
    }
    else
    {
        bool restart_needed;
        CLDS_SORTED_LIST_HANDLE bucket_list = NULL;
        HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, value);
        uint64_t hash;
        BUCKET_ARRAY* current_bucket;
        volatile LONG bucket_count;
        uint64_t bucket_index;

        // find or allocate a new bucket array
        // always insert in the first bucket array
        current_bucket = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        bucket_count = InterlockedAdd(&current_bucket->bucket_count, 0);
        while (InterlockedAdd(&current_bucket->item_count, 0) >= bucket_count)
        {
            // allocate a new bucket array
            BUCKET_ARRAY* new_bucket_array = (BUCKET_ARRAY*)malloc(sizeof(BUCKET_ARRAY) + (sizeof(CLDS_SORTED_LIST_HANDLE) * bucket_count * 2));
            if (new_bucket_array == NULL)
            {
                // cannot allocate new bucket, will stick to what we have, but do not fail
                break;
            }
            else
            {
                LONG i;
                BUCKET_ARRAY* first_bucket_array = current_bucket;

                // insert new bucket
                bucket_count = bucket_count * 2;
                (void)InterlockedExchange(&new_bucket_array->bucket_count, bucket_count);
                (void)InterlockedExchange(&new_bucket_array->item_count, 0);

                // initialize buckets
                for (i = 0; i < bucket_count; i++)
                {
                    (void)InterlockedExchangePointer(&new_bucket_array->hash_table[i], NULL);
                }

                (void)InterlockedExchangePointer((volatile PVOID*)&new_bucket_array->next_bucket, first_bucket_array);
                if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, new_bucket_array, first_bucket_array) == first_bucket_array)
                {
                    current_bucket = new_bucket_array;
                    break;
                }
                else
                {
                    // first bucket changed, drop ours and use the one that was inserted
                    free(new_bucket_array);

                    current_bucket = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
                    bucket_count = InterlockedAdd(&current_bucket->bucket_count, 0);
                }
            }
        }

        (void)InterlockedIncrement(&current_bucket->item_count);

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_038: [ `clds_hash_table_insert` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
        hash = clds_hash_table->compute_hash(key);
            
        // find the bucket
        bucket_index = hash % bucket_count;

        do
        {
            // do we have a list here or do we create one?
            bucket_list = InterlockedCompareExchangePointer(&current_bucket->hash_table[bucket_index], NULL, NULL);
            if (bucket_list != NULL)
            {
                restart_needed = false;
            }
            else
            {
                // create a list
                /* Codes_SRS_CLDS_HASH_TABLE_01_019: [ If no sorted list exists at the determined bucket index then a new list shall be created. ]*/
                bucket_list = clds_sorted_list_create(clds_hash_table->clds_hazard_pointers, get_item_key_cb, clds_hash_table, key_compare_cb, clds_hash_table, clds_hash_table->sequence_number, sorted_list_skipped_seq_no, clds_hash_table);
                if (bucket_list == NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
                    LogError("Cannot allocate list for hash table bucket");
                    restart_needed = false;
                }
                else
                {
                    // now put the list in the bucket
                    if (InterlockedCompareExchangePointer(&current_bucket->hash_table[bucket_index], bucket_list, NULL) != NULL)
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
            result = CLDS_HASH_TABLE_INSERT_ERROR;
        }
        else
        {
            CLDS_SORTED_LIST_INSERT_RESULT list_insert_result;

            /* Codes_SRS_CLDS_HASH_TABLE_01_020: [ A new sorted list item shall be created by calling `clds_sorted_list_node_create`. ]*/
            hash_table_item->key = key;

            /* Codes_SRS_CLDS_HASH_TABLE_01_021: [ The new sorted list node shall be inserted in the sorted list at the identified bucket by calling `clds_sorted_list_insert`. ]*/
            /* Codes_SRS_CLDS_HASH_TABLE_01_059: [ For each insert the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_insert`. ]*/
            list_insert_result = clds_sorted_list_insert(bucket_list, clds_hazard_pointers_thread, (void*)value, sequence_number);
            
            if (list_insert_result == CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS)
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_046: [ If the key already exists in the hash table, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ALREADY_EXISTS`. ]*/
                LogError("Key already exists in hash table");
                result = CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS;
            }
            else if (list_insert_result != CLDS_SORTED_LIST_INSERT_OK)
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. ]*/
                LogError("Cannot insert hash table item into list");
                result = CLDS_HASH_TABLE_INSERT_ERROR;
            }
            else
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_009: [ On success `clds_hash_table_insert` shall return `CLDS_HASH_TABLE_INSERT_OK`. ]*/
                result = CLDS_HASH_TABLE_INSERT_OK;
                goto all_ok;
            }
        }
    }

all_ok:
    return result;
}

bool find_by_key_value(void* item_compare_context, CLDS_SORTED_LIST_ITEM* item)
{
    bool result;
    FIND_BY_KEY_VALUE_CONTEXT* find_by_key_value_context = (FIND_BY_KEY_VALUE_CONTEXT*)item_compare_context;
    HASH_TABLE_ITEM* hash_table_item = (HASH_TABLE_ITEM*)CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);

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

    /* Codes_SRS_CLDS_HASH_TABLE_01_015: [ If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. ]*/
    if ((clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_017: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_016: [ If `key` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_066: [ If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL)))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p, clds_hazard_pointers_thread = %p, sequence_number = %p",
            clds_hash_table, key, clds_hazard_pointers_thread, sequence_number);
        result = CLDS_HASH_TABLE_DELETE_ERROR;
    }
    else
    {
        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_039: [ `clds_hash_table_delete` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;

        // always insert in the first bucket array
        current_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&current_bucket_array->next_bucket, NULL, NULL);

            if (InterlockedAdd(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % InterlockedAdd(&current_bucket_array->bucket_count, 0);

                bucket_list = InterlockedCompareExchangePointer(&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_NOT_FOUND`. ]*/
                    result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_DELETE_RESULT list_delete_result;

                    /* Codes_SRS_CLDS_HASH_TABLE_01_063: [ For each delete the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_delete_key`. ]*/
                    list_delete_result = clds_sorted_list_delete_key(bucket_list, clds_hazard_pointers_thread, key, sequence_number);
                    if (list_delete_result == CLDS_SORTED_LIST_DELETE_NOT_FOUND)
                    {
                        // not found
                        /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_NOT_FOUND`. ]*/
                    }
                    else if (list_delete_result == CLDS_SORTED_LIST_DELETE_OK)
                    {
                        (void)InterlockedDecrement(&current_bucket_array->item_count);

                        /* Codes_SRS_CLDS_HASH_TABLE_01_014: [ On success `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_OK`. ]*/
                        result = CLDS_HASH_TABLE_DELETE_OK;
                        break;
                    }
                    else
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_024: [ If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_ERROR`. ]*/
                        result = CLDS_HASH_TABLE_DELETE_ERROR;
                        break;
                    }
                }
            }

            current_bucket_array = next_bucket_array;
        }
    }

    return result;
}

CLDS_HASH_TABLE_DELETE_RESULT clds_hash_table_delete_key_value(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_DELETE_RESULT result;

    if ((clds_hash_table == NULL) ||
        (key == NULL) ||
        (value == NULL) ||
        (clds_hazard_pointers_thread == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p, value = %p, clds_hazard_pointers_thread = %p, sequence_number = %p",
            clds_hash_table, key, value, clds_hazard_pointers_thread, sequence_number);
        result = CLDS_HASH_TABLE_DELETE_ERROR;
    }
    else
    {
        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        // compute the hash
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;

        // always insert in the first bucket array
        current_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&current_bucket_array->next_bucket, NULL, NULL);

            if (InterlockedAdd(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % InterlockedAdd(&current_bucket_array->bucket_count, 0);

                bucket_list = InterlockedCompareExchangePointer(&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    result = CLDS_HASH_TABLE_DELETE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_DELETE_RESULT list_delete_result;
                    list_delete_result = clds_sorted_list_delete_item(bucket_list, clds_hazard_pointers_thread, (void*)value, sequence_number);
                    if (list_delete_result == CLDS_SORTED_LIST_DELETE_NOT_FOUND)
                    {
                        // not found
                    }
                    else if (list_delete_result == CLDS_SORTED_LIST_DELETE_OK)
                    {
                        (void)InterlockedDecrement(&current_bucket_array->item_count);

                        result = CLDS_HASH_TABLE_DELETE_OK;
                        break;
                    }
                    else
                    {
                        result = CLDS_HASH_TABLE_DELETE_ERROR;
                        break;
                    }
                }
            }

            current_bucket_array = next_bucket_array;
        }
    }

    return result;
}

CLDS_HASH_TABLE_REMOVE_RESULT clds_hash_table_remove(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM** item, int64_t* sequence_number)
{
    CLDS_HASH_TABLE_DELETE_RESULT result;

    /* Codes_SRS_CLDS_HASH_TABLE_01_050: [ If `clds_hash_table` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
    if ((clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_052: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_051: [ If `key` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_056: [ If `item` is NULL, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_070: [ If the `sequence_number` argument is non-NULL, but no start sequence number was specified in `clds_hash_table_create`, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
        ((sequence_number != NULL) && (clds_hash_table->sequence_number == NULL)))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p, clds_hazard_pointers_thread = %p, item = %p, sequence_number = %p",
            clds_hash_table, key, clds_hazard_pointers_thread, item, sequence_number);
        result = CLDS_HASH_TABLE_REMOVE_ERROR;
    }
    else
    {
        CLDS_SORTED_LIST_HANDLE bucket_list;
        BUCKET_ARRAY* current_bucket_array;

        /* Codes_SRS_CLDS_HASH_TABLE_01_047: [ `clds_hash_table_remove` shall remove a key from the hash table and return a pointer to the item to the user. ]*/

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_048: [ `clds_hash_table_remove` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        result = CLDS_HASH_TABLE_REMOVE_NOT_FOUND;

        // always insert in the first bucket array
        current_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&current_bucket_array->next_bucket, NULL, NULL);

            if (InterlockedAdd(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                uint64_t bucket_index = hash % InterlockedAdd(&current_bucket_array->bucket_count, 0);

                bucket_list = InterlockedCompareExchangePointer(&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list == NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_053: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), `clds_hash_table_remove` shall return `CLDS_HASH_TABLE_REMOVE_NOT_FOUND`. ]*/
                    result = CLDS_HASH_TABLE_REMOVE_NOT_FOUND;
                }
                else
                {
                    CLDS_SORTED_LIST_REMOVE_RESULT list_remove_result;
                    /* Codes_SRS_CLDS_HASH_TABLE_01_067: [ For each remove the order of the operation shall be computed by passing `sequence_number` to `clds_sorted_list_remove_key`. ]*/
                    list_remove_result = clds_sorted_list_remove_key(bucket_list, clds_hazard_pointers_thread, key, (void*)item, sequence_number);
                    if (list_remove_result == CLDS_SORTED_LIST_REMOVE_NOT_FOUND)
                    {
                        // not found
                    }
                    else if (list_remove_result == CLDS_SORTED_LIST_REMOVE_OK)
                    {
                        (void)InterlockedDecrement(&current_bucket_array->item_count);

                        /* Codes_SRS_CLDS_HASH_TABLE_01_049: [ On success `clds_hash_table_remove` shall return `CLDS_HASH_TABLE_REMOVE_OK`. ]*/
                        result = CLDS_HASH_TABLE_REMOVE_OK;
                        break;
                    }
                    else
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_054: [ If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_remove` shall fail and return `CLDS_HASH_TABLE_REMOVE_ERROR`. ]*/
                        result = CLDS_HASH_TABLE_REMOVE_ERROR;
                        break;
                    }
                }
            }

            /* Codes_SRS_CLDS_HASH_TABLE_01_055: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
            current_bucket_array = next_bucket_array;
        }
    }

    return result;
}

CLDS_HASH_TABLE_ITEM* clds_hash_table_find(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_HASH_TABLE_ITEM* result;

    /* Codes_SRS_CLDS_HASH_TABLE_01_035: [ If `clds_hash_table` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
    if ((clds_hash_table == NULL) ||
        /* Tests_SRS_CLDS_HASH_TABLE_01_036: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_037: [ If `key` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
        (key == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p", clds_hash_table, key);
        result = NULL;
    }
    else
    {
        CLDS_SORTED_LIST_HANDLE bucket_list;

        result = NULL;

        // compute the hash
        /* Codes_SRS_CLDS_HASH_TABLE_01_040: [ `clds_hash_table_find` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
        uint64_t hash = clds_hash_table->compute_hash(key);

        // always insert in the first bucket array
        /* Codes_SRS_CLDS_HASH_TABLE_01_041: [ `clds_hash_table_find` shall look up the key in the biggest array of buckets. ]*/
        BUCKET_ARRAY* current_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        while (current_bucket_array != NULL)
        {
            BUCKET_ARRAY* next_bucket_array = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&current_bucket_array->next_bucket, NULL, NULL);

            if (InterlockedAdd(&current_bucket_array->item_count, 0) != 0)
            {
                // find the bucket
                /* Codes_SRS_CLDS_HASH_TABLE_01_044: [ Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling `clds_sorted_list_find`. ]*/
                uint64_t bucket_index = hash % InterlockedAdd(&current_bucket_array->bucket_count, 0);

                bucket_list = InterlockedCompareExchangePointer(&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_034: [ `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. ]*/
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
            /* Codes_SRS_CLDS_HASH_TABLE_01_043: [ If the key is not found at all, `clds_hash_table_find` shall return NULL. ]*/
            result = NULL;
        }
        else
        {
            // all OK
        }
    }

    return result;
}

CLDS_HASH_TABLE_ITEM* clds_hash_table_node_create(size_t node_size, HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("Failed allocating memory");
    }
    else
    {
        volatile CLDS_HASH_TABLE_ITEM* item = (volatile CLDS_HASH_TABLE_ITEM*)((unsigned char*)result);
        HASH_TABLE_ITEM* hash_table_item = CLDS_SORTED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);
        hash_table_item->item_cleanup_callback = item_cleanup_callback;
        hash_table_item->item_cleanup_callback_context = item_cleanup_callback_context;
        item->item.item_cleanup_callback = sorted_list_item_cleanup;
        item->item.item_cleanup_callback_context = (void*)item;
        (void)InterlockedExchange(&item->item.ref_count, 1);
    }

    return result;
}

int clds_hash_table_node_inc_ref(CLDS_HASH_TABLE_ITEM* item)
{
    int result;

    if (item == NULL)
    {
        LogError("NULL item");
        result = __FAILURE__;
    }
    else
    {
        if (clds_sorted_list_node_inc_ref((void*)item) != 0)
        {
            LogError("clds_sorted_list_node_inc_ref failed");
            result = __FAILURE__;
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
        LogError("NULL item");
    }
    else
    {
        clds_sorted_list_node_release((void*)item);
    }
}
