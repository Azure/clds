// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_atomics.h"

/* this is a lock free singly linked list implementation */

typedef struct CLDS_SINGLY_LINKED_LIST_TAG
{
    int dummy;
} CLDS_SINGLY_LINKED_LIST;

CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list_create(void)
{
    CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list = (CLDS_SINGLY_LINKED_LIST_HANDLE)malloc(sizeof(CLDS_SINGLY_LINKED_LIST));
    if (clds_singly_linked_list == NULL)
    {
        LogError("Cannot allocate memory for the singly linked list");
    }
    else
    {
        // all ok
    }

    return clds_singly_linked_list;
}

void clds_hash_table_destroy(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list)
{
    if (clds_singly_linked_list == NULL)
    {
        LogError("NULL clds_singly_linked_list");
    }
    else
    {
        free(clds_singly_linked_list);
    }
}
