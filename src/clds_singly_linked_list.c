// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "azure_c_util/gballoc.h"
#include "azure_c_util/xlogging.h"
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

static void internal_node_destroy(CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    if (InterlockedDecrement(&item->ref_count) == 0)
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
        if (item->item_cleanup_callback != NULL)
        {
            /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_singly_linked_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
            item->item_cleanup_callback(item->item_cleanup_callback_context, item);
        }

        free((void*)item);
    }
}

static void reclaim_list_node(void* node)
{
    internal_node_destroy((CLDS_SINGLY_LINKED_LIST_ITEM*)node);
}

static CLDS_SINGLY_LINKED_LIST_DELETE_RESULT internal_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    CLDS_SINGLY_LINKED_LIST_DELETE_RESULT result = CLDS_SINGLY_LINKED_LIST_DELETE_ERROR;

    bool restart_needed;

    do
    {
        // check that the node is really in the list and obtain
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SINGLY_LINKED_LIST_ITEM* previous_item = NULL;
        volatile CLDS_SINGLY_LINKED_LIST_ITEM** current_item_address = &clds_singly_linked_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SINGLY_LINKED_LIST_ITEM* current_item = (volatile CLDS_SINGLY_LINKED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
            if (current_item == NULL)
            {
                if (previous_hp != NULL)
                {
                    // let go of previous hazard pointer
                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                }

                restart_needed = false;

                /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_018: [ If the item does not exist in the list, clds_singly_linked_list_delete shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND. ]*/
                /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_024: [ If no item matches the criteria, clds_singly_linked_list_delete_if shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND. ]*/
                result = CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND;
                break;
            }
            else
            {
                // acquire hazard pointer
                CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                if (current_item_hp == NULL)
                {
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    LogError("Cannot acquire hazard pointer");
                    restart_needed = false;
                    result = MU_FAILURE;
                    break;
                }
                else
                {
                    // now make sure the item has not changed
                    if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

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
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

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

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_026: [ On success, clds_singly_linked_list_delete shall return CLDS_SINGLY_LINKED_LIST_DELETE_OK. ]*/
                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_025: [ On success, clds_singly_linked_list_delete_if shall return CLDS_SINGLY_LINKED_LIST_DELETE_OK. ]*/
                                        result = CLDS_SINGLY_LINKED_LIST_DELETE_OK;

                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // someone is deleting our left node, restart, but first unlock our own delete mark
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_026: [ On success, clds_singly_linked_list_delete shall return CLDS_SINGLY_LINKED_LIST_DELETE_OK. ]*/
                                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_025: [ On success, clds_singly_linked_list_delete_if shall return CLDS_SINGLY_LINKED_LIST_DELETE_OK. ]*/
                                        result = CLDS_SINGLY_LINKED_LIST_DELETE_OK;

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
    CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list;

    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_003: [ If clds_hazard_pointers is NULL, clds_singly_linked_list_create shall fail and return NULL. ]*/
    if (clds_hazard_pointers == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p", clds_hazard_pointers);
        clds_singly_linked_list = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_001: [ clds_singly_linked_list_create shall create a new singly linked list object and on success it shall return a non-NULL handle to the newly created list. ]*/
        clds_singly_linked_list = (CLDS_SINGLY_LINKED_LIST_HANDLE)malloc(sizeof(CLDS_SINGLY_LINKED_LIST));
        if (clds_singly_linked_list == NULL)
        {
            /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_002: [ If any error happens, clds_singly_linked_list_create shall fail and return NULL. ]*/
            LogError("Cannot allocate memory for the singly linked list");
        }
        else
        {
            // all ok
            clds_singly_linked_list->clds_hazard_pointers = clds_hazard_pointers;

            (void)InterlockedExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL);
        }
    }

    return clds_singly_linked_list;
}

void clds_singly_linked_list_destroy(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list)
{
    if (clds_singly_linked_list == NULL)
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_005: [ If clds_singly_linked_list is NULL, clds_singly_linked_list_destroy shall return. ]*/
        LogError("Invalid arguments: CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list=%p", clds_singly_linked_list);
    }
    else
    {
        CLDS_SINGLY_LINKED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_singly_linked_list->head, NULL, NULL);

        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
        // go through all the items and free them
        while (current_item != NULL)
        {
            CLDS_SINGLY_LINKED_LIST_ITEM* next_item = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

            /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_040: [ For each item that is freed, the callback item_cleanup_callback passed to clds_singly_linked_list_node_create shall be called, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
            /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_041: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the freed items. ]*/
            internal_node_destroy(current_item);
            current_item = next_item;
        } 

        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_004: [ clds_singly_linked_list_destroy shall free all resources associated with the singly linked list instance. ]*/
        free(clds_singly_linked_list);
    }
}

int clds_singly_linked_list_insert(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result;

    (void)clds_hazard_pointers_thread;

    if (
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_011: [ If clds_singly_linked_list is NULL, clds_singly_linked_list_insert shall fail and return a non-zero value. ]*/
        (clds_singly_linked_list == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_012: [ If item is NULL, clds_singly_linked_list_insert shall fail and return a non-zero value. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_013: [ If clds_hazard_pointers_thread is NULL, clds_singly_linked_list_insert shall fail and return a non-zero value. ]*/
        (clds_hazard_pointers_thread == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, CLDS_SINGLY_LINKED_LIST_ITEM* item=%p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item);
        result = MU_FAILURE;
    }
    else
    {
        bool restart_needed;

        do
        {
            // get current head
            /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_009: [ clds_singly_linked_list_insert inserts an item in the list. ]*/
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

        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_010: [ On success clds_singly_linked_list_insert shall return 0. ]*/
        result = 0;
    }

    return result;
}

CLDS_SINGLY_LINKED_LIST_DELETE_RESULT clds_singly_linked_list_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    CLDS_SINGLY_LINKED_LIST_DELETE_RESULT result;

    if (
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_015: [ If clds_singly_linked_list is NULL, clds_singly_linked_list_delete shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (clds_singly_linked_list == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_016: [ If clds_hazard_pointers_thread is NULL, clds_singly_linked_list_delete shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_017: [ If item is NULL, clds_singly_linked_list_delete shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (item == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, CLDS_SINGLY_LINKED_LIST_ITEM* item=%p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item);
        result = CLDS_SINGLY_LINKED_LIST_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_014: [ clds_singly_linked_list_delete deletes an item from the list by its pointer. ]*/
        result = internal_delete(clds_singly_linked_list, clds_hazard_pointers_thread, compare_item_by_ptr, item);
    }

    return result;
}

CLDS_SINGLY_LINKED_LIST_DELETE_RESULT clds_singly_linked_list_delete_if(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    CLDS_SINGLY_LINKED_LIST_DELETE_RESULT result;

    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_023: [ item_compare_callback_context shall be allowed to be NULL. ]*/

    if (
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_020: [ If clds_singly_linked_list is NULL, clds_singly_linked_list_delete_if shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (clds_singly_linked_list == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_021: [ If clds_hazard_pointers_thread is NULL, clds_singly_linked_list_delete_if shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_022: [ If item_compare_callback is NULL, clds_singly_linked_list_delete_if shall fail and return CLDS_SINGLY_LINKED_LIST_DELETE_ERROR. ]*/
        (item_compare_callback == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback=%p, void* item_compare_callback_context=%p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item_compare_callback, item_compare_callback_context);
        result = CLDS_SINGLY_LINKED_LIST_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_019: [ clds_singly_linked_list_delete_if deletes an item that matches the criteria given by a user compare function. ]*/
        result = internal_delete(clds_singly_linked_list, clds_hazard_pointers_thread, item_compare_callback, item_compare_callback_context);
    }

    return result;
}

CLDS_SINGLY_LINKED_LIST_ITEM* clds_singly_linked_list_find(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context)
{
    CLDS_SINGLY_LINKED_LIST_ITEM* result;

    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_032: [ item_compare_callback_context shall be allowed to be NULL. ]*/

    if (
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_028: [ If clds_singly_linked_list is NULL, clds_singly_linked_list_find shall fail and return NULL. ]*/
        (clds_singly_linked_list == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_030: [ If clds_hazard_pointers_thread is NULL, clds_singly_linked_list_find shall fail and return NULL. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_031: [ If item_compare_callback is NULL, clds_singly_linked_list_find shall fail and return NULL. ]*/
        (item_compare_callback == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback=%p, void* item_compare_callback_context=%p",
            clds_singly_linked_list, clds_hazard_pointers_thread, item_compare_callback, item_compare_callback_context);
        result = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_027: [ clds_singly_linked_list_find shall find in the list the first item that matches the criteria given by a user compare function. ]*/

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
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    restart_needed = false;

                    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_singly_linked_list_find shall fail and return NULL. ]*/
                    result = NULL;
                    break;
                }
                else
                {
                    // acquire hazard pointer
                    CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                    if (current_item_hp == NULL)
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

                        LogError("Cannot acquire hazard pointer");
                        restart_needed = false;

                        /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_singly_linked_list_find shall fail and return NULL. ]*/
                        result = NULL;
                        break;
                    }
                    else
                    {
                        // now make sure the item has not changed
                        if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                        {
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            if (item_compare_callback(item_compare_callback_context, (CLDS_SINGLY_LINKED_LIST_ITEM*)current_item))
                            {
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                // found it
                                /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_034: [ clds_singly_linked_list_find shall return a pointer to the item with the reference count already incremented so that it can be safely used by the caller. ]*/
                                (void)InterlockedIncrement(&current_item->ref_count);
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_029: [ On success clds_singly_linked_list_find shall return a non-NULL pointer to the found linked list item. ]*/
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

CLDS_SINGLY_LINKED_LIST_ITEM* clds_singly_linked_list_node_create(size_t node_size, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_036: [ item_cleanup_callback shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SINGLY_LINKED_LIST_01_037: [ item_cleanup_callback_context shall be allowed to be NULL. ]*/

    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("malloc failed");
    }
    else
    {
        volatile CLDS_SINGLY_LINKED_LIST_ITEM* item = (volatile CLDS_SINGLY_LINKED_LIST_ITEM*)((unsigned char*)result);
        item->item_cleanup_callback = item_cleanup_callback;
        item->item_cleanup_callback_context = item_cleanup_callback_context;
        (void)InterlockedExchange(&item->ref_count, 1);
        (void)InterlockedExchangePointer((volatile PVOID*)&item->next, NULL);
    }

    return result;
}

int clds_singly_linked_list_node_inc_ref(CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    int result;

    if (item == NULL)
    {
        LogError("INvalid arguments: CLDS_SINGLY_LINKED_LIST_ITEM* item=%p", item);
        result = MU_FAILURE;
    }
    else
    {
        (void)InterlockedIncrement(&item->ref_count);
        result = 0;
    }

    return result;
}

void clds_singly_linked_list_node_release(CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    if (item == NULL)
    {
        LogError("INvalid arguments: CLDS_SINGLY_LINKED_LIST_ITEM* item=%p", item);
    }
    else
    {
        internal_node_destroy(item);
    }
}
