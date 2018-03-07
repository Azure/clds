// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hash_table.h"
#include "clds/clds_atomics.h"

/* this is a hash table implementation */

typedef struct CLDS_HASH_TABLE_TAG
{
    uint64_t bucket_count;
} CLDS_HASH_TABLE;

CLDS_HASH_TABLE_HANDLE clds_hash_table_create(void)
{
    CLDS_HASH_TABLE_HANDLE clds_hash_table = (CLDS_HASH_TABLE_HANDLE)malloc(sizeof(CLDS_HASH_TABLE));
    if (clds_hash_table == NULL)
    {
        LogError("Cannot allocate memory for hash table");
    }
    else
    {
        // all OK
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

        result = 0;
    }

    return result;
}
