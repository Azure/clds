// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_st_hash_set.h"

/* this is a hash set implementation that is single threaded (not thread safe) */

typedef struct HASH_TABLE_ITEM_TAG
{
    struct HASH_TABLE_ITEM_TAG* next;
    void* key;
} HASH_TABLE_ITEM;

typedef struct CLDS_ST_HASH_SET_TAG
{
    size_t bucket_count;
    CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash;
    HASH_TABLE_ITEM** hash_set;
} CLDS_ST_HASH_SET;

CLDS_ST_HASH_SET_HANDLE clds_st_hash_set_create(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size)
{
    CLDS_ST_HASH_SET_HANDLE clds_st_hash_set;

    (void)key_compare_func;

    if ((compute_hash == NULL) ||
        (initial_bucket_size == 0))
    {
        LogError("Zero initial bucket size");
    }
    else
    {
        clds_st_hash_set = (CLDS_ST_HASH_SET_HANDLE)malloc(sizeof(CLDS_ST_HASH_SET));
        if (clds_st_hash_set == NULL)
        {
            LogError("Cannot allocate memory for hash table");
        }
        else
        {
            clds_st_hash_set->hash_set = malloc(sizeof(void*) * initial_bucket_size);
            if (clds_st_hash_set->hash_set == NULL)
            {
                LogError("Cannot allocate memory for hash set array");
            }
            else
            {
                size_t i;

                // all OK
                clds_st_hash_set->compute_hash = compute_hash;

                // set the initial bucket count
                clds_st_hash_set->bucket_count = initial_bucket_size;

                for (i = 0; i < initial_bucket_size; i++)
                {
                    clds_st_hash_set->hash_set[i] = NULL;
                }

                goto all_ok;
            }

            free(clds_st_hash_set);
        }
    }

    clds_st_hash_set = NULL;

all_ok:
    return clds_st_hash_set;
}

void clds_st_hash_set_destroy(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set)
{
    if (clds_st_hash_set == NULL)
    {
        /* Codes_SRS_CLDS_HASH_TABLE_01_007: [ If `clds_st_hash_set` is NULL, `clds_st_hash_set_destroy` shall return. ]*/
        LogError("NULL clds_st_hash_set");
    }
    else
    {
        size_t i;

        /* Codes_SRS_CLDS_HASH_TABLE_01_006: [ `clds_st_hash_set_destroy` shall free all resources associated with the hash table instance. ]*/
        for (i = 0; i < clds_st_hash_set->bucket_count; i++)
        {
            HASH_TABLE_ITEM* hash_table_item = clds_st_hash_set->hash_set[i];
            while (hash_table_item != NULL)
            {
                HASH_TABLE_ITEM* next_item = hash_table_item->next;
                free(hash_table_item);
                hash_table_item = next_item;
            }
        }

        free(clds_st_hash_set->hash_set);
        free(clds_st_hash_set);
    }
}

CLDS_ST_HASH_SET_INSERT_RESULT clds_st_hash_set_insert(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key)
{
    CLDS_ST_HASH_SET_INSERT_RESULT result;

    if ((clds_st_hash_set == NULL) ||
        (key == NULL))
    {
        LogError("Invalid arguments: CLDS_ST_HASH_SET_HANDLE clds_st_hash_set=%p, void* key=%p", clds_st_hash_set, key);
        result = CLDS_ST_HASH_SET_INSERT_ERROR;
    }
    else
    {
        // compute the hash
        uint64_t hash = clds_st_hash_set->compute_hash(key);
            
        // find the bucket
        uint64_t bucket_index = hash % clds_st_hash_set->bucket_count;

        HASH_TABLE_ITEM* hash_table_item = (HASH_TABLE_ITEM*)malloc(sizeof(HASH_TABLE_ITEM));
        if (hash_table_item == NULL)
        {
            LogError("Error allocating memory for hash set item");
            result = CLDS_ST_HASH_SET_INSERT_ERROR;
        }
        else
        {
            hash_table_item->key = key;
            hash_table_item->next = clds_st_hash_set->hash_set[bucket_index];
            clds_st_hash_set->hash_set[bucket_index] = hash_table_item;

            result = CLDS_ST_HASH_SET_INSERT_OK;
        }
    }

    return result;
}

CLDS_ST_HASH_SET_FIND_RESULT clds_st_hash_set_find(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key)
{
    CLDS_ST_HASH_SET_FIND_RESULT result;

    if ((clds_st_hash_set == NULL) ||
        (key == NULL))
    {
        LogError("Invalid arguments: CLDS_ST_HASH_SET_HANDLE clds_st_hash_set=%p, void* key=%p", clds_st_hash_set, key);
        result = CLDS_ST_HASH_SET_FIND_ERROR;
    }
    else
    {
        // compute the hash
        uint64_t hash = clds_st_hash_set->compute_hash(key);

        // find the bucket
        uint64_t bucket_index = hash % clds_st_hash_set->bucket_count;

        HASH_TABLE_ITEM* hash_table_item = clds_st_hash_set->hash_set[bucket_index];
        while (hash_table_item != NULL)
        {
            if (hash_table_item->key == key)
            {
                break;
            }

            hash_table_item = hash_table_item->next;
        }

        if (hash_table_item == NULL)
        {
            result = CLDS_ST_HASH_SET_FIND_NOT_FOUND;
        }
        else
        {
            result = CLDS_ST_HASH_SET_FIND_OK;
        }
    }

    return result;
}
