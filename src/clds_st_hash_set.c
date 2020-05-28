// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include "azure_c_util/gballoc.h"
#include "azure_c_logging/xlogging.h"
#include "clds/clds_st_hash_set.h"

/* this is a hash set implementation that is single threaded (not thread safe) */

typedef struct HASH_TABLE_ITEM_TAG
{
    struct HASH_TABLE_ITEM_TAG* next;
    void* key;
} HASH_TABLE_ITEM;

typedef struct CLDS_ST_HASH_SET_TAG
{
    CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash_func;
    CLDS_ST_HASH_SET_KEY_COMPARE_FUNC key_compare_func;
    size_t bucket_count;
    HASH_TABLE_ITEM** hash_set;
} CLDS_ST_HASH_SET;

CLDS_ST_HASH_SET_HANDLE clds_st_hash_set_create(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash_func, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC key_compare_func, size_t bucket_size)
{
    CLDS_ST_HASH_SET_HANDLE clds_st_hash_set;

    if (
        /* Codes_SRS_CLDS_ST_HASH_SET_01_003: [ If compute_hash_func is NULL, clds_st_hash_set_create shall fail and return NULL. ]*/
        (compute_hash_func == NULL) ||
        /* Codes_SRS_CLDS_ST_HASH_SET_01_004: [ If key_compare_func is NULL, clds_st_hash_set_create shall fail and return NULL. ]*/
        (key_compare_func == NULL) ||
        /* Codes_SRS_CLDS_ST_HASH_SET_01_005: [ If bucket_size is 0, clds_st_hash_set_create shall fail and return NULL. ]*/
        (bucket_size == 0)
        )
    {
        LogError("Invalid arguments: CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash=%p, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC key_compare_func=%p, size_t bucket_size=%zu",
            compute_hash_func, key_compare_func, bucket_size);
    }
    else
    {
        /* Codes_SRS_CLDS_ST_HASH_SET_01_001: [ clds_st_hash_set_create shall create a new hash set object and on success it shall return a non-NULL handle to the newly created hash set. ]*/
        clds_st_hash_set = (CLDS_ST_HASH_SET_HANDLE)malloc(sizeof(CLDS_ST_HASH_SET));
        if (clds_st_hash_set == NULL)
        {
            /* Codes_SRS_CLDS_ST_HASH_SET_01_002: [ If any error happens, clds_st_hash_set_create shall fail and return NULL. ]*/
            LogError("Cannot allocate memory for hash table");
        }
        else
        {
            /* Codes_SRS_CLDS_ST_HASH_SET_01_022: [ clds_st_hash_set_create shall allocate memory for the array of buckets used to store the hash set data. ]*/
            clds_st_hash_set->hash_set = malloc(sizeof(void*) * bucket_size);
            if (clds_st_hash_set->hash_set == NULL)
            {
                /* Codes_SRS_CLDS_ST_HASH_SET_01_002: [ If any error happens, clds_st_hash_set_create shall fail and return NULL. ]*/
                LogError("Cannot allocate memory for hash set array");
            }
            else
            {
                size_t i;

                clds_st_hash_set->compute_hash_func = compute_hash_func;
                clds_st_hash_set->key_compare_func = key_compare_func;

                // set the initial bucket count
                clds_st_hash_set->bucket_count = bucket_size;

                for (i = 0; i < bucket_size; i++)
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
        /* Codes_SRS_CLDS_ST_HASH_SET_01_007: [ If clds_st_hash_set is NULL, clds_st_hash_set_destroy shall return. ]*/
        LogError("Invalid arguments: clds_st_hash_set=%p", clds_st_hash_set);
    }
    else
    {
        size_t i;

        /* Codes_SRS_CLDS_HASH_TABLE_01_006: [ clds_st_hash_set_destroy shall free all resources associated with the hash table instance. ]*/
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

        /* Codes_SRS_CLDS_ST_HASH_SET_01_006: [ clds_st_hash_set_destroy shall free all resources associated with the hash set instance. ]*/
        free(clds_st_hash_set->hash_set);
        free(clds_st_hash_set);
    }
}

CLDS_ST_HASH_SET_INSERT_RESULT clds_st_hash_set_insert(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key)
{
    CLDS_ST_HASH_SET_INSERT_RESULT result;

    if (
        /* Codes_SRS_CLDS_ST_HASH_SET_01_010: [ If clds_st_hash_set is NULL, clds_st_hash_set_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        (clds_st_hash_set == NULL) ||
        /* Codes_SRS_CLDS_ST_HASH_SET_01_011: [ If key is NULL, clds_st_hash_set_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
        (key == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_ST_HASH_SET_HANDLE clds_st_hash_set=%p, void* key=%p", clds_st_hash_set, key);
        result = CLDS_ST_HASH_SET_INSERT_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_ST_HASH_SET_01_012: [ clds_st_hash_set_insert shall hash the key by calling the compute_hash function passed to clds_st_hash_set_create. ]*/
        uint64_t hash = clds_st_hash_set->compute_hash_func(key);
            
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
            /* Codes_SRS_CLDS_ST_HASH_SET_01_008: [ clds_st_hash_set_insert shall insert a key in the hash set. ]*/
            hash_table_item->key = key;
            hash_table_item->next = clds_st_hash_set->hash_set[bucket_index];
            clds_st_hash_set->hash_set[bucket_index] = hash_table_item;

            /* Codes_SRS_CLDS_ST_HASH_SET_01_009: [ On success clds_st_hash_set_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
            result = CLDS_ST_HASH_SET_INSERT_OK;
        }
    }

    return result;
}

CLDS_ST_HASH_SET_FIND_RESULT clds_st_hash_set_find(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key)
{
    CLDS_ST_HASH_SET_FIND_RESULT result;

    if (
        (clds_st_hash_set == NULL) ||
        (key == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_ST_HASH_SET_HANDLE clds_st_hash_set=%p, void* key=%p", clds_st_hash_set, key);
        result = CLDS_ST_HASH_SET_FIND_ERROR;
    }
    else
    {
        // compute the hash
        uint64_t hash = clds_st_hash_set->compute_hash_func(key);

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
