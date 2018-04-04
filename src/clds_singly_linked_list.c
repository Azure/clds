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
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    volatile CLDS_SINGLY_LINKED_LIST_ITEM* head;
} CLDS_SINGLY_LINKED_LIST;

static bool compare_item_by_ptr(void* item_compare_context, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    bool result;

    if (item == item_compare_context)
    {
        result = true;
    }
    else
    {
        result = false;
    }

    return result;
}

static void internal_destroy(CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    if (InterlockedDecrement(&item->ref_count) == 0)
    {
        free((void*)item);
    }
}

static void reclaim_list_node(void* node)
{
    internal_destroy((CLDS_SINGLY_LINKED_LIST_ITEM*)node);
}

static int internal_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    int result = 0;

    // check that the node is really in the list and obtain
    bool restart_needed;

    do
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SINGLY_LINKED_LIST_ITEM* previous_item = NULL;
        volatile CLDS_SINGLY_LINKED_LIST_ITEM** current_item_address = &clds_singly_linked_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SINGLY_LINKED_LIST_ITEM* current_item = (volatile CLDS_SINGLY_LINKED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
            if (current_item == NULL)
            {
                LogError("Not found");
                restart_needed = false;
                result = __FAILURE__;
                break;
            }
            else
            {
                // acquire hazard pointer
                CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                if (current_item_hp == NULL)
                {
                    LogError("Cannot acquire hazard pointer");
                    restart_needed = false;
                    result = __FAILURE__;
                    break;
                }
                else
                {
                    // now make sure the item has not changed
                    if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                    {
                        // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                        restart_needed = true;
                        break;
                    }
                    else
                    {
                        if (item_compare_callback(item_compare_callback_context, (CLDS_SINGLY_LINKED_LIST_ITEM*)current_item))
                        {
                            // mark the node as deleted
                            // get the next pointer as this is the only place where we keep information
                            volatile CLDS_SINGLY_LINKED_LIST_ITEM* current_next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                            // mark that the node is deleted
                            if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)current_next | 1), (PVOID)current_next) != (PVOID)current_next)
                            {
                                // restart
                                restart_needed = true;
                                break;
                            }
                            else
                            {
                                // the current node is marked for deletion, now try to change the previous link to the next value

                                // If in the meanwhile someone would be deleting node A they would have to first set the
                                // deleted flag on it, in which case we'd see the CAS fail

                                if (previous_item == NULL)
                                {
                                    // we are removing the head
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // head changed, restart
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);
                                        restart_needed = false;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // someone is deleting our left node, restart, but first unlock our own delete mark
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        if (previous_hp != NULL)
                                        {
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        }

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            // we have a stable pointer to the current item, now simply set the previous to be this
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            previous_hp = current_item_hp;
                            previous_item = current_item;
                            current_item_address = (volatile CLDS_SINGLY_LINKED_LIST_ITEM**)&current_item->next;
                        }
                    }
                }
            }
        } while (1);
    } while (restart_needed);

    return result;
}

CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list_create(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list = (CLDS_SINGLY_LINKED_LIST_HANDLE)malloc(sizeof(CLDS_SINGLY_LINKED_LIST));
    if (clds_singly_linked_list == NULL)
    {
        LogError("Cannot allocate memory for the singly linked list");
    }
    else
    {
        // all ok
        clds_singly_linked_list->clds_hazard_pointers = clds_hazard_pointers;
        (void)InterlockedExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL);
    }

    return clds_singly_linked_list;
}

void clds_singly_linked_list_destroy(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    if (clds_singly_linked_list == NULL)
    {
        LogError("NULL clds_singly_linked_list");
    }
    else
    {
        CLDS_SINGLY_LINKED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL, NULL);

        // go through all the items and free them
        while (current_item != NULL)
        {
            CLDS_SINGLY_LINKED_LIST_ITEM* next_item = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);
            item_cleanup_callback(item_cleanup_callback_context, current_item);
            current_item = next_item;
        } 

        free(clds_singly_linked_list);
    }
}

int clds_singly_linked_list_insert(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result;

    (void)clds_hazard_pointers_thread;

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

int clds_singly_linked_list_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result;

    if ((clds_singly_linked_list == NULL) ||
        (clds_hazard_pointers_thread == NULL))
    {
        LogError("Invalid arguments: clds_singly_linked_list = %p, clds_hazard_pointers_thread = %p, item = %p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item);
        result = __FAILURE__;
    }
    else
    {
        result = internal_delete(clds_singly_linked_list, clds_hazard_pointers_thread, compare_item_by_ptr, item);
    }

    return result;
}

int clds_singly_linked_list_delete_if(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    int result;

    if ((clds_singly_linked_list == NULL) ||
        (clds_hazard_pointers_thread == NULL) ||
        (item_compare_callback == NULL))
    {
        LogError("Invalid arguments: clds_singly_linked_list = %p, clds_hazard_pointers_thread = %p, item_compare_callback = %p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item_compare_callback);
        result = __FAILURE__;
    }
    else
    {
        result = internal_delete(clds_singly_linked_list, clds_hazard_pointers_thread, item_compare_callback, item_compare_callback_context);
    }

    return result;
}

CLDS_SINGLY_LINKED_LIST_ITEM* clds_singly_linked_list_find(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    CLDS_SINGLY_LINKED_LIST_ITEM* result;

    if ((clds_singly_linked_list == NULL) ||
        (item_compare_callback == NULL))
    {
        LogError("Invalid arguments: clds_singly_linked_list = %p, item_compare_callback = %p",
            clds_singly_linked_list, item_compare_callback);
        result = NULL;
    }
    else
    {
        // check that the node is really in the list and obtain
        bool restart_needed;
        result = NULL;

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SINGLY_LINKED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SINGLY_LINKED_LIST_ITEM** current_item_address = &clds_singly_linked_list->head;

            do
            {
                // get the current_item value
                volatile CLDS_SINGLY_LINKED_LIST_ITEM* current_item = (volatile CLDS_SINGLY_LINKED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
                if (current_item == NULL)
                {
                    LogError("Not found");
                    restart_needed = false;
                    result = NULL;
                    break;
                }
                else
                {
                    // acquire hazard pointer
                    CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                    if (current_item_hp == NULL)
                    {
                        LogError("Cannot acquire hazard pointer");
                        restart_needed = false;
                        result = NULL;
                        break;
                    }
                    else
                    {
                        // now make sure the item has not changed
                        if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                        {
                            // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            if (item_compare_callback(item_compare_callback_context, (CLDS_SINGLY_LINKED_LIST_ITEM*)current_item))
                            {
                                // found it
                                current_item->ref_count++;
                                result = (CLDS_SINGLY_LINKED_LIST_ITEM*)current_item;
                                restart_needed = false;
                                break;
                            }
                            else
                            {
                                // we have a stable pointer to the current item, now simply set the previous to be this
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                previous_hp = current_item_hp;
                                previous_item = current_item;
                                current_item_address = (volatile CLDS_SINGLY_LINKED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);
    }

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
        internal_destroy(item);
    }
}
