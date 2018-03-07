// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "windows.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hash_table.h"
#include "clds/clds_atomics.h"

/* this is a hash table implementation */

typedef struct CLDS_HASH_TABLE_TAG
{
    volatile LONG item_count_until_resize;
    volatile LONG bucket_count;
    COMPUTE_HASH_FUNC compute_hash;
    void** hash_table;
} CLDS_HASH_TABLE;

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table = (CLDS_HASH_TABLE_HANDLE)malloc(sizeof(CLDS_HASH_TABLE));
    if (clds_hash_table == NULL)
    {
        LogError("Cannot allocate memory for hash table");
    }
    else
    {
        clds_hash_table->hash_table = malloc(sizeof(void*) * 1000000);
        if (clds_hash_table->hash_table == NULL)
        {
            LogError("Cannot allocate memory for hash table array");
        }
        else
        {
            // all OK
            clds_hash_table->compute_hash = compute_hash;

            // set the initial bucket count
            (void)InterlockedExchange(&clds_hash_table->item_count_until_resize, 1000000);
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

int clds_hash_table_insert(CLDS_HASH_TABLE_HANDLE clds_hash_table, void* key, void* value)
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
            LogInfo("Table rresize needed");
            result = __FAILURE__;
        }
        else
        {
            // compute the hash
            uint64_t hash = clds_hash_table->compute_hash(key);
            
            // find the bucket
            uint64_t bucket_index = hash % clds_hash_table->bucket_count;

            // check that we already have an item in the bucket
            if (InterlockedCompareExchangePointer(&clds_hash_table->hash_table[bucket_index], value, NULL) != NULL)
            {
                // collision
                LogInfo("Collision");
                result = __FAILURE__;
            }
            else
            {
                // value stored
                result = 0;
            }
        }
    }

    return result;
}
