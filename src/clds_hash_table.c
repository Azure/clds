// Licensed under the MIT license.See LICENSE file in the project root for full license information.

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

typedef struct CLDS_HASH_TABLE_TAG
{
    volatile LONG item_count_until_resize;
    volatile LONG bucket_count;
    COMPUTE_HASH_FUNC compute_hash;
    CLDS_SINGLY_LINKED_LIST_HANDLE* hash_table;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
} CLDS_HASH_TABLE;

typedef struct HASH_TABLE_ITEM_TAG
{
    void* key;
    void* value;
} HASH_TABLE_ITEM;

DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(HASH_TABLE_ITEM);

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table;

    if (initial_bucket_size == 0)
    {
        LogError("Zero initial bucket size");
        clds_hash_table = NULL;
    }
    else
    {
        clds_hash_table = (CLDS_HASH_TABLE_HANDLE)malloc(sizeof(CLDS_HASH_TABLE));
        if (clds_hash_table == NULL)
        {
            LogError("Cannot allocate memory for hash table");
        }
        else
        {
            clds_hash_table->hash_table = malloc(sizeof(void*) * initial_bucket_size);
            if (clds_hash_table->hash_table == NULL)
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
                (void)InterlockedExchange(&clds_hash_table->item_count_until_resize, 1000000);
                (void)InterlockedExchange(&clds_hash_table->bucket_count, initial_bucket_size);

                for (i = 0; i < initial_bucket_size; i++)
                {
                    (void)InterlockedExchangePointer(&clds_hash_table->hash_table[i], NULL);
                }
            }
        }
    }

    return clds_hash_table;
}

void clds_hash_table_destroy(CLDS_HASH_TABLE_HANDLE clds_hash_table)
{
    if (clds_hash_table == NULL)
    {
        LogError("NULL clds_hash_table");
    }
    else
    {
        free(clds_hash_table->hash_table);
        free(clds_hash_table);
    }
}

int clds_hash_table_insert(CLDS_HASH_TABLE_HANDLE clds_hash_table, void* key, void* value, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    int result;

    if ((clds_hash_table == NULL) ||
        (key == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p", clds_hash_table, key);
        result = __FAILURE__;
    }
    else
    {
        (void)value;

        // check if we should resize first
        if (InterlockedDecrement(&clds_hash_table->item_count_until_resize) == 0)
        {
            // resize the table
            LogInfo("Table resize needed");
            result = __FAILURE__;
        }
        else
        {
            bool restart_needed;
            CLDS_SINGLY_LINKED_LIST_HANDLE bucket_list = NULL;

            // compute the hash
            uint64_t hash = clds_hash_table->compute_hash(key);
            
            // find the bucket
            uint64_t bucket_index = hash % InterlockedAdd(&clds_hash_table->bucket_count, 0);

            do
            {
                // do we have a list here or do we create one?
                bucket_list = InterlockedCompareExchangePointer(&clds_hash_table->hash_table[bucket_index], NULL, NULL);
                if (bucket_list != NULL)
                {
                    restart_needed = false;
                }
                else
                {
                    // create a list
                    bucket_list = clds_singly_linked_list_create(clds_hash_table->clds_hazard_pointers);
                    if (bucket_list == NULL)
                    {
                        LogError("Cannot allocate list for hash table bucket");
                        restart_needed = false;
                    }
                    else
                    {
                        // now put the list in the bucket
                        if (InterlockedCompareExchangePointer(&clds_hash_table->hash_table[bucket_index], bucket_list, NULL) != NULL)
                        {
                            // oops, someone else inserted a new list, just bail on our list and restart
                            clds_singly_linked_list_destroy(bucket_list, NULL, NULL);
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
                CLDS_SINGLY_LINKED_LIST_ITEM* list_item = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(HASH_TABLE_ITEM);
                if (list_item == NULL)
                {
                    LogError("Cannot create hash table entry item");
                    result = __FAILURE__;
                }
                else
                {
                    HASH_TABLE_ITEM* hash_table_item = (HASH_TABLE_ITEM*)CLDS_SINGLY_LINKED_LIST_GET_VALUE(HASH_TABLE_ITEM, list_item);
                    hash_table_item->key = key;
                    hash_table_item->value = value;

                    if (clds_singly_linked_list_insert(bucket_list, list_item, clds_hazard_pointers_thread) != 0)
                    {
                        LogError("Cannot insert hash table item into list");
                        result = __FAILURE__;
                    }
                    else
                    {
                        result = 0;
                        goto all_ok;
                    }

                    free(hash_table_item);
                }
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

    if (strcmp(hash_table_item->key, item_compare_context) != 0)
    {
        result = false;
    }
    else
    {
        result = true;
    }

    return result;
}

int clds_hash_table_delete(CLDS_HASH_TABLE_HANDLE clds_hash_table, void* key, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    int result;

    if ((clds_hash_table == NULL) ||
        (key == NULL))
    {
        LogError("Invalid arguments: clds_hash_table = %p, key = %p", clds_hash_table, key);
        result = __FAILURE__;
    }
    else
    {
        CLDS_SINGLY_LINKED_LIST_HANDLE bucket_list;

        // compute the hash
        uint64_t hash = clds_hash_table->compute_hash(key);

        // find the bucket
        uint64_t bucket_index = hash % InterlockedAdd(&clds_hash_table->bucket_count, 0);

        bucket_list = InterlockedCompareExchangePointer(&clds_hash_table->hash_table[bucket_index], NULL, NULL);
        if (bucket_list == NULL)
        {
            // not found
            result = __FAILURE__;
        }
        else
        {
            // now put the list in the bucket
            CLDS_SINGLY_LINKED_LIST_ITEM* list_item = clds_singly_linked_list_find(bucket_list, clds_hazard_pointers_thread, find_by_key, key);
            if (list_item == NULL)
            {
                // not found
                result = __FAILURE__;
            }
            else
            {
                if (clds_singly_linked_list_delete(bucket_list, list_item, clds_hazard_pointers_thread) != 0)
                {
                    LogError("Error deleting item");
                    result = __FAILURE__;
                }
                else
                {
                    result = 0;
                }
            }
        }
    }

    return result;
}
