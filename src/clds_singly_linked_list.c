// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_atomics.h"
#include "clds/clds_hazard_pointers.h"

/* this is a lock free singly linked list implementation */

typedef struct CLDS_SINGLY_LINKED_LIST_TAG
{
    volatile CLDS_SINGLY_LINKED_LIST_ITEM* head;
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
        (void)InterlockedExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL);
    }

    return clds_singly_linked_list;
}

void clds_singly_linked_list_destroy(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list)
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

int clds_singly_linked_list_insert(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result;

    if (clds_singly_linked_list == NULL)
    {
        LogError("NULL clds_singly_linked_list");
        result = __FAILURE__;
    }
    else
    {
        bool restart_needed;

        do
        {
            // get current head
            item->next = (CLDS_SINGLY_LINKED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL, NULL);

            if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, item, (PVOID)item->next) != item->next)
            {
                restart_needed = true;
            }
            else
            {
                restart_needed = false;
            }
        } while (restart_needed);

        result = 0;
    }

    return result;
}

int clds_singly_linked_list_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result = 0;

    (void)clds_singly_linked_list;
    (void)item;

    return result;
}

CLDS_SINGLY_LINKED_LIST_ITEM* clds_singly_linked_list_node_create(size_t node_size, size_t item_offset, size_t record_offset)
{
    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("Failed allocating memory");
    }
    else
    {
        volatile CLDS_SINGLY_LINKED_LIST_ITEM* item = (volatile CLDS_SINGLY_LINKED_LIST_ITEM*)((unsigned char*)result + item_offset);
        item->record_offset = record_offset;
        (void)InterlockedExchange(&item->ref_count, 1);
        (void)InterlockedExchangePointer((volatile PVOID*)&item->next, NULL);
    }

    return result;
}

void clds_singly_linked_list_node_destroy(CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    if (item == NULL)
    {
        LogError("NULL item_ptr");
    }
    else
    {
        if (InterlockedDecrement(&item->ref_count) == 0)
        {
            free((void*)item);
        }
    }
}
