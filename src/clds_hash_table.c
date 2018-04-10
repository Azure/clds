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
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_hazard_pointers.h"

/* this is a hash table implementation */

typedef struct BUCKET_ARRAY_TAG
{
    volatile struct BUCKET_ARRAY_TAG* next_bucket;
    volatile LONG bucket_count;
    volatile LONG item_count;
    CLDS_SINGLY_LINKED_LIST_HANDLE hash_table[];
} BUCKET_ARRAY;

typedef struct CLDS_HASH_TABLE_TAG
{
    COMPUTE_HASH_FUNC compute_hash;
    volatile BUCKET_ARRAY* first_hash_table;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
} CLDS_HASH_TABLE;

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table;

    /* Codes_SRS_CLDS_HASH_TABLE_01_003: [ If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
    if ((compute_hash == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_004: [ If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. ]*/
        (initial_bucket_size == 0) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_005: [ If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
        (clds_hazard_pointers == NULL))
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, `clds_hash_table_create` shall fail and return NULL. ]*/
        LogError("Zero initial bucket size");
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
            clds_hash_table->first_hash_table = malloc(sizeof(BUCKET_ARRAY) + (sizeof(CLDS_SINGLY_LINKED_LIST_HANDLE) * initial_bucket_size));
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

static void singly_linked_list_item_cleanup(void* context, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    HASH_TABLE_ITEM* hash_table_item = CLDS_SINGLY_LINKED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);

    (void)context;
    if (hash_table_item->item_cleanup_callback != NULL)
    {
        hash_table_item->item_cleanup_callback(hash_table_item->item_cleanup_callback_context, (void*)hash_table_item);
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
                    CLDS_SINGLY_LINKED_LIST_HANDLE linked_list = InterlockedCompareExchangePointer(&bucket_array->hash_table[i], NULL, NULL);
                    if (linked_list != NULL)
                    {
                        clds_singly_linked_list_destroy(linked_list);
                    }
                }
            }

            free(bucket_array);
            bucket_array = next_bucket_array;
        }

        free(clds_hash_table);
    }
}

int clds_hash_table_insert(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, HASH_TABLE_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    int result;

    /* Codes_SRS_CLDS_HASH_TABLE_01_028: [ `item_cleanup_callback` shall be allowed to be NULL. ]*/

    /* Codes_SRS_CLDS_HASH_TABLE_01_010: [ If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
    if ((clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_011: [ If `key` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_012: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
        (clds_hazard_pointers_thread == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p", clds_hash_table, key);
        result = __FAILURE__;
    }
    else
    {
        bool restart_needed;
        CLDS_SINGLY_LINKED_LIST_HANDLE bucket_list = NULL;
        HASH_TABLE_ITEM* hash_table_item = CLDS_SINGLY_LINKED_LIST_GET_VALUE(HASH_TABLE_ITEM, value);
        uint64_t hash;
        BUCKET_ARRAY* current_bucket;
        volatile LONG bucket_count;
        uint64_t bucket_index;

        hash_table_item->item_cleanup_callback = item_cleanup_callback;
        hash_table_item->item_cleanup_callback_context = item_cleanup_callback_context;

        // find or allocate a new bucket array
        // always insert in the first bucket array
        current_bucket = (BUCKET_ARRAY*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hash_table->first_hash_table, NULL, NULL);
        bucket_count = InterlockedAdd(&current_bucket->bucket_count, 0);
        while (InterlockedAdd(&current_bucket->item_count, 0) >= bucket_count)
        {
            // allocate a new bucket array
            BUCKET_ARRAY* new_bucket_array = (BUCKET_ARRAY*)malloc(sizeof(BUCKET_ARRAY) + (sizeof(CLDS_SINGLY_LINKED_LIST_HANDLE) * bucket_count * 2));
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
                /* Codes_SRS_CLDS_HASH_TABLE_01_019: [ If no singly linked list exists at the determined bucket index then a new list shall be created. ]*/
                bucket_list = clds_singly_linked_list_create(clds_hash_table->clds_hazard_pointers);
                if (bucket_list == NULL)
                {
                    LogError("Cannot allocate list for hash table bucket");
                    restart_needed = false;
                }
                else
                {
                    // now put the list in the bucket
                    if (InterlockedCompareExchangePointer(&current_bucket->hash_table[bucket_index], bucket_list, NULL) != NULL)
                    {
                        // oops, someone else inserted a new list, just bail on our list and restart
                        clds_singly_linked_list_destroy(bucket_list);
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
            result = __FAILURE__;
        }
        else
        {
            /* Codes_SRS_CLDS_HASH_TABLE_01_020: [ A new singly linked list item shall be created by calling `clds_singly_linked_list_node_create`. ]*/
            hash_table_item->key = key;

            /* Codes_SRS_CLDS_HASH_TABLE_01_021: [ The new singly linked list node shall be inserted in the singly linked list at the identified bucket by calling `clds_singly_linked_list_insert`. ]*/
            if (clds_singly_linked_list_insert(bucket_list, clds_hazard_pointers_thread, (void*)value) != 0)
            {
                LogError("Cannot insert hash table item into list");
                result = __FAILURE__;
            }
            else
            {
                /* Codes_SRS_CLDS_HASH_TABLE_01_009: [ On success `clds_hash_table_insert` shall return 0. ]*/
                result = 0;
                goto all_ok;
            }
        }
    }

all_ok:
    return result;
}

bool find_by_key(void* item_compare_context, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    bool result;
    HASH_TABLE_ITEM* hash_table_item = (HASH_TABLE_ITEM*)CLDS_SINGLY_LINKED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);

    if (hash_table_item->key != item_compare_context)
    {
        result = false;
    }
    else
    {
        result = true;
    }

    return result;
}

CLDS_HASH_TABLE_DELETE_RESULT clds_hash_table_delete(CLDS_HASH_TABLE_HANDLE clds_hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_HASH_TABLE_DELETE_RESULT result;

    /* Codes_SRS_CLDS_HASH_TABLE_01_015: [ If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
    if ((clds_hash_table == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_016: [ If `key` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_HASH_TABLE_01_017: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p", clds_hash_table, key);
        result = __FAILURE__;
    }
    else
    {
        CLDS_SINGLY_LINKED_LIST_HANDLE bucket_list;
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
                    /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table, `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_NOT_FOUND`. ]*/
                    result = __FAILURE__;
                }
                else
                {
                    CLDS_SINGLY_LINKED_LIST_DELETE_RESULT delete_result = clds_singly_linked_list_delete_if(bucket_list, clds_hazard_pointers_thread, find_by_key, key);
                    if (delete_result == CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND)
                    {
                        // not found
                        /* Codes_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table, `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_NOT_FOUND`. ]*/
                    }
                    else if (delete_result == CLDS_SINGLY_LINKED_LIST_DELETE_OK)
                    {
                        (void)InterlockedDecrement(&current_bucket_array->item_count);

                        /* Codes_SRS_CLDS_HASH_TABLE_01_014: [ On success `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_OK`. ]*/
                        result = CLDS_HASH_TABLE_DELETE_OK;
                        break;
                    }
                    else
                    {
                        /* Codes_SRS_CLDS_HASH_TABLE_01_024: [ If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
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
        CLDS_SINGLY_LINKED_LIST_HANDLE bucket_list;

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
                /* Codes_SRS_CLDS_HASH_TABLE_01_044: [ Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling `clds_singly_linked_list_find`. ]*/
                uint64_t bucket_index = hash % InterlockedAdd(&current_bucket_array->bucket_count, 0);

                bucket_list = InterlockedCompareExchangePointer(&current_bucket_array->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    /* Codes_SRS_CLDS_HASH_TABLE_01_034: [ `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. ]*/
                    result = (void*)clds_singly_linked_list_find(bucket_list, clds_hazard_pointers_thread, find_by_key, key);
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

CLDS_HASH_TABLE_ITEM* clds_hash_table_node_create(size_t node_size)
{
    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("Failed allocating memory");
    }
    else
    {
        volatile CLDS_HASH_TABLE_ITEM* item = (volatile CLDS_HASH_TABLE_ITEM*)((unsigned char*)result);
        HASH_TABLE_ITEM* hash_table_item = CLDS_SINGLY_LINKED_LIST_GET_VALUE(HASH_TABLE_ITEM, item);
        hash_table_item->item_cleanup_callback = NULL;
        hash_table_item->item_cleanup_callback_context = NULL;
        item->item.item_cleanup_callback = NULL;
        item->item.item_cleanup_callback_context = NULL;
        (void)InterlockedExchange(&item->item.ref_count, 1);
    }

    return result;
}

void clds_hash_table_node_destroy(CLDS_HASH_TABLE_ITEM* item)
{
    if (item == NULL)
    {
        LogError("NULL item");
    }
    else
    {
        clds_singly_linked_list_node_destroy((void*)item);
    }
}
