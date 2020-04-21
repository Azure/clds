// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "windows.h"

#include "azure_c_util/gballoc.h"
#include "azure_c_util/xlogging.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_atomics.h"
#include "clds/clds_hazard_pointers.h"

MU_DEFINE_ENUM_STRINGS(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_RESULT_VALUES);

/* this is a lock free sorted list implementation */

typedef struct CLDS_SORTED_LIST_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    volatile CLDS_SORTED_LIST_ITEM* head;
    SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb;
    void* get_item_key_cb_context;
    SORTED_LIST_KEY_COMPARE_CB key_compare_cb;
    void* key_compare_cb_context;
    volatile LONG64* sequence_number;
    SORTED_LIST_SKIPPED_SEQ_NO_CB skipped_seq_no_cb;
    void* skipped_seq_no_cb_context;

    // Support for locking the list for writes
    volatile LONG locked_for_write;
    volatile LONG pending_write_operations;
} CLDS_SORTED_LIST;

typedef int(*SORTED_LIST_ITEM_COMPARE_CB)(void* context, CLDS_SORTED_LIST_ITEM* item1, void* item_compare_target);

static int compare_item_by_ptr(void* context, CLDS_SORTED_LIST_ITEM* item, void* item_compare_target)
{
    int result;

    (void)context;

    if (item == item_compare_target)
    {
        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
}

static int compare_item_by_key(void* context, CLDS_SORTED_LIST_ITEM* item, void* item_compare_target)
{
    CLDS_SORTED_LIST_HANDLE clds_sorted_list = (CLDS_SORTED_LIST_HANDLE)context;
    // get item key
    void* item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, item);
    return clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, item_key, item_compare_target);
}

static void internal_node_destroy(CLDS_SORTED_LIST_ITEM* item)
{
    if (InterlockedDecrement(&item->ref_count) == 0)
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
        if (item->item_cleanup_callback != NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
            item->item_cleanup_callback(item->item_cleanup_callback_context, item);
        }

        free((void*)item);
    }
}

static void reclaim_list_node(void* node)
{
    internal_node_destroy((CLDS_SORTED_LIST_ITEM*)node);
}

static void check_lock_and_begin_write_operation(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    ULONG locked_for_write;
    do
    {
        (void)InterlockedIncrement(&clds_sorted_list->pending_write_operations);
        locked_for_write = InterlockedAdd(&clds_sorted_list->locked_for_write, 0);
        if (locked_for_write != 0)
        {
            (void)InterlockedDecrement(&clds_sorted_list->pending_write_operations);
            WakeByAddressAll((void*)&clds_sorted_list->pending_write_operations);

            // Wait for unlock
            (void)WaitOnAddress(&clds_sorted_list->locked_for_write, &locked_for_write, sizeof(locked_for_write), INFINITE);
        }
    } while (locked_for_write != 0);
}

static void end_write_operation(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    (void)InterlockedDecrement(&clds_sorted_list->pending_write_operations);
    WakeByAddressAll((void*)&clds_sorted_list->pending_write_operations);
}

static void internal_lock_writes(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    /*Codes_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
    (void)InterlockedIncrement(&clds_sorted_list->locked_for_write);

    /*Codes_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
    ULONG pending_writes;
    do
    {
        pending_writes = InterlockedAdd(&clds_sorted_list->pending_write_operations, 0);
        if (pending_writes != 0)
        {
            // Wait for writes
            (void)WaitOnAddress(&clds_sorted_list->pending_write_operations, &pending_writes, sizeof(pending_writes), INFINITE);
        }
    } while (pending_writes != 0);
}

static void internal_unlock_writes(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    /*Codes_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
    (void)InterlockedDecrement(&clds_sorted_list->locked_for_write);
    WakeByAddressAll((void*)&clds_sorted_list->locked_for_write);
}

static CLDS_SORTED_LIST_DELETE_RESULT internal_delete(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SORTED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_target, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_DELETE_RESULT result = CLDS_SORTED_LIST_DELETE_ERROR;

    bool restart_needed;


    do
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
        // start at the head of the list and scan through all nodes until we find the one we are looking for
        volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);

            // if the current item is NULL (do not look at the lock bit), we are done.
            if ((void*)((uintptr_t)current_item & ~0x1) == NULL)
            {
                if (previous_hp != NULL)
                {
                    // let go of previous hazard pointer
                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                }

                restart_needed = false;

                /* Codes_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
                /* Codes_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
                result = CLDS_SORTED_LIST_DELETE_NOT_FOUND;
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
                    result = CLDS_SORTED_LIST_DELETE_ERROR;
                    break;
                }
                else
                {
                    // now make sure the item has not changed (if it has changed, then it means we can not touch the memory)
                    if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL) != (volatile PVOID*)((uintptr_t)current_item & ~0x1))
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
                        // clear any delete lock flag, since we ensured it is not set
                        current_item = (volatile CLDS_SORTED_LIST_ITEM*)((uintptr_t)current_item & ~0x1);

                        int compare_result = item_compare_callback(clds_sorted_list, (CLDS_SORTED_LIST_ITEM*)current_item, item_compare_target);
                        if (compare_result == 0)
                        {
                            // mark the node as deleted
                            // get the next pointer as this is the only place where we keep information
                            volatile CLDS_SORTED_LIST_ITEM* current_next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                            // mark that the node is deleted by setting the lock delete bit
                            if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)current_next | 0x1), (PVOID)((uintptr_t)current_next & ~0x1)) != (PVOID)((uintptr_t)current_next & ~0x1))
                            {
                                // could not set the lock delete bit (some other thread modified the next value, we shall restart)
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
                                int64_t local_seq_no = 0;

                                // clear any delete lock flag, since we ensured it is not set
                                current_next = (volatile CLDS_SORTED_LIST_ITEM*)((uintptr_t)current_next & ~0x1);

                                /* Codes_SRS_CLDS_SORTED_LIST_01_070: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
                                /* Codes_SRS_CLDS_SORTED_LIST_01_071: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
                                if (clds_sorted_list->sequence_number != NULL)
                                {
                                    local_seq_no = InterlockedIncrement64(clds_sorted_list->sequence_number);

                                    // get a new seq no and stamp it on the node to be deleted, if any other thread deletes they will alter the seq no.
                                    (void)InterlockedExchange64(&current_item->seq_no, local_seq_no);
                                }

                                // the current node is marked for deletion, now try to change the previous link to the next value
                                // the state of the list looks like below:
                                // (Prev) ----> (Current)

                                // If in the meanwhile someone would be deleting node Prev they would have to first set the
                                // deleted flag on it, in which case we'd see the CAS for changing the Prev->next pointer fail

                                if (previous_item == NULL)
                                {
                                    // we are removing the head, special case
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // head changed, restart, but make sure we unlock the delete bit for current node
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        if (clds_sorted_list->sequence_number != NULL)
                                        {
                                            int64_t temp_seq_no = InterlockedAdd64(&current_item->seq_no, 0);
                                            if (temp_seq_no != local_seq_no)
                                            {
                                                if (clds_sorted_list->skipped_seq_no_cb != NULL)
                                                {
                                                    clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, local_seq_no);
                                                }
                                            }

                                            /* Codes_SRS_CLDS_SORTED_LIST_01_064: [ If the sequence_number argument passed to clds_sorted_list_delete is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
                                            /* Codes_SRS_CLDS_SORTED_LIST_01_067: [ If the sequence_number argument passed to clds_sorted_list_delete_key is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
                                            if (sequence_number != NULL)
                                            {
                                                // since we deleted the node, simply pick up the current sequence number (has to be greater than the insert)
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_063: [ For each delete the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_066: [ For each delete key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                *sequence_number = temp_seq_no;
                                            }
                                        }

                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)current_item, reclaim_list_node);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
                                        result = CLDS_SORTED_LIST_DELETE_OK;

                                        break;
                                    }
                                }
                                else
                                {
                                    // not removing the head, set Prev->next to current_next, but clear the lock delete bit from the next pointer!
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
                                        if (clds_sorted_list->sequence_number != NULL)
                                        {
                                            int64_t temp_seq_no = InterlockedAdd64(&current_item->seq_no, 0);
                                            if (temp_seq_no != local_seq_no)
                                            {
                                                if (clds_sorted_list->skipped_seq_no_cb != NULL)
                                                {
                                                    clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, local_seq_no);
                                                }
                                            }

                                            /* Codes_SRS_CLDS_SORTED_LIST_01_064: [ If the sequence_number argument passed to clds_sorted_list_delete is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
                                            /* Codes_SRS_CLDS_SORTED_LIST_01_067: [ If the sequence_number argument passed to clds_sorted_list_delete_key is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
                                            if (sequence_number != NULL)
                                            {
                                                // since we deleted the node, simply pick up the current sequence number (has to be greater than the insert)
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_063: [ For each delete the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_066: [ For each delete key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                *sequence_number = temp_seq_no;
                                            }
                                        }


                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)current_item, reclaim_list_node);

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
                                        result = CLDS_SORTED_LIST_DELETE_OK;

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
                            current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                        }
                    }
                }
            }
        } while (1);
    } while (restart_needed);

    return result;
}

static CLDS_SORTED_LIST_REMOVE_RESULT internal_remove(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SORTED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_target, CLDS_SORTED_LIST_ITEM** item, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_REMOVE_RESULT result = CLDS_SORTED_LIST_DELETE_ERROR;

    // check that the node is really in the list and obtain
    bool restart_needed;

    do
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
        volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
            if (current_item == NULL)
            {
                if (previous_hp != NULL)
                {
                    // let go of previous hazard pointer
                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                }

                restart_needed = false;

                /* Codes_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_NOT_FOUND. ]*/
                result = CLDS_SORTED_LIST_REMOVE_NOT_FOUND;
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
                    result = CLDS_SORTED_LIST_REMOVE_ERROR;
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
                        // clear any delete lock bit, since we validated it should not be there
                        current_item = (void*)((uintptr_t)current_item & ~0x1);

                        int compare_result = item_compare_callback(clds_sorted_list, (CLDS_SORTED_LIST_ITEM*)current_item, item_compare_target);
                        if (compare_result == 0)
                        {
                            // mark the node as deleted
                            // get the next pointer as this is the only place where we keep information
                            volatile CLDS_SORTED_LIST_ITEM* current_next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                            // mark that the node is deleted
                            if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)current_next | 1), (PVOID)((uintptr_t)current_next & ~0x1)) != (PVOID)((uintptr_t)current_next & ~0x1))
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
                                // clear any delete lock bit, since we validated it should not be there
                                current_next = (void*)((uintptr_t)current_next & ~0x1);

                                int64_t local_seq_no = 0;

                                /* Codes_SRS_CLDS_SORTED_LIST_01_073: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
                                if (clds_sorted_list->sequence_number != NULL)
                                {
                                    local_seq_no = InterlockedIncrement64(clds_sorted_list->sequence_number);

                                    // get a new seq no and stamp it on the node to be deleted, if any other thread deletes they will alter the seq no.
                                    /* Codes_SRS_CLDS_SORTED_LIST_01_074: [ If the sequence_number argument passed to clds_sorted_list_remove_key is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. ]*/
                                    (void)InterlockedExchange64(&current_item->seq_no, local_seq_no);
                                }

                                // the current node is marked for deletion, now try to change the previous link to the next value

                                // If in the meanwhile someone would be deleting node A they would have to first set the
                                // deleted flag on it, in which case we'd see the CAS fail

                                if (previous_item == NULL)
                                {
                                    // we are removing the head
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // head changed, restart
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
                                        *item = (CLDS_SORTED_LIST_ITEM*)current_item;
                                        clds_sorted_list_node_inc_ref(*item);

                                        if (clds_sorted_list->sequence_number != NULL)
                                        {
                                            int64_t temp_seq_no = InterlockedAdd64(&current_item->seq_no, 0);
                                            if (temp_seq_no != local_seq_no)
                                            {
                                                if (clds_sorted_list->skipped_seq_no_cb != NULL)
                                                {
                                                    clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, local_seq_no);
                                                }
                                            }

                                            if (sequence_number != NULL)
                                            {
                                                // since we deleted the node, simply pick up the current sequence number (has to be greater than the insert)
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_072: [ For each remove key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                *sequence_number = temp_seq_no;
                                            }
                                        }

                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)current_item, reclaim_list_node);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
                                        result = CLDS_SORTED_LIST_REMOVE_OK;

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
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
                                        *item = (CLDS_SORTED_LIST_ITEM*)current_item;
                                        clds_sorted_list_node_inc_ref(*item);

                                        if (clds_sorted_list->sequence_number != NULL)
                                        {
                                            int64_t temp_seq_no = InterlockedAdd64(&current_item->seq_no, 0);
                                            if (temp_seq_no != local_seq_no)
                                            {
                                                if (clds_sorted_list->skipped_seq_no_cb != NULL)
                                                {
                                                    clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, local_seq_no);
                                                }
                                            }

                                            if (sequence_number != NULL)
                                            {
                                                // since we deleted the node, simply pick up the current sequence number (has to be greater than the insert)
                                                /* Codes_SRS_CLDS_SORTED_LIST_01_072: [ For each remove key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
                                                *sequence_number = temp_seq_no;
                                            }
                                        }

                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)current_item, reclaim_list_node);

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
                                        result = CLDS_SORTED_LIST_REMOVE_OK;

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
                            current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                        }
                    }
                }
            }
        } while (1);
    } while (restart_needed);

    return result;
}

CLDS_SORTED_LIST_HANDLE clds_sorted_list_create(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb, void* get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB key_compare_cb, void* key_compare_cb_context, volatile int64_t* start_sequence_number, SORTED_LIST_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context)
{
    CLDS_SORTED_LIST_HANDLE clds_sorted_list;

    /* Codes_SRS_CLDS_SORTED_LIST_01_049: [ get_item_key_cb_context shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_050: [ key_compare_cb_context shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_076: [ skipped_seq_no_cb shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_077: [ skipped_seq_no_cb_context shall be allowed to be NULL. ]*/

    /* Codes_SRS_CLDS_SORTED_LIST_01_059: [ start_sequence_number shall be allowed to be NULL, in which case no order shall be provided for the operations. ]*/

    if (
        /* Codes_SRS_CLDS_SORTED_LIST_01_003: [ If clds_hazard_pointers is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
        (clds_hazard_pointers == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_045: [ If get_item_key_cb is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
        (get_item_key_cb == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_046: [ If key_compare_cb is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
        (key_compare_cb == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_078: [ If start_sequence_number is NULL, then skipped_seq_no_cb must also be NULL, otherwise clds_sorted_list_create shall fail and return NULL. ]*/
        ((start_sequence_number == NULL) && (skipped_seq_no_cb != NULL))
        )
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p, SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb=%p, void* get_item_key_cb_context=%p, SORTED_LIST_KEY_COMPARE_CB key_compare_cb=%p, void* key_compare_cb_context=%p, volatile int64_t* start_sequence_number=%p, SORTED_LIST_SKIPPED_SEQ_NO_CB skipped_seq_no_cb=%p, void* skipped_seq_no_cb_context=%p",
            clds_hazard_pointers, get_item_key_cb, get_item_key_cb_context, key_compare_cb, key_compare_cb_context, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
        clds_sorted_list = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_001: [ clds_sorted_list_create shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
        clds_sorted_list = (CLDS_SORTED_LIST_HANDLE)malloc(sizeof(CLDS_SORTED_LIST));
        if (clds_sorted_list == NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_002: [ If any error happens, clds_sorted_list_create shall fail and return NULL. ]*/
            LogError("malloc failed");
        }
        else
        {
            // all ok
            clds_sorted_list->clds_hazard_pointers = clds_hazard_pointers;
            clds_sorted_list->get_item_key_cb = get_item_key_cb;
            clds_sorted_list->get_item_key_cb_context = get_item_key_cb_context;
            clds_sorted_list->key_compare_cb = key_compare_cb;
            clds_sorted_list->key_compare_cb_context = key_compare_cb_context;
            clds_sorted_list->skipped_seq_no_cb = skipped_seq_no_cb;
            clds_sorted_list->skipped_seq_no_cb_context = skipped_seq_no_cb_context;

            (void)InterlockedExchange(&clds_sorted_list->locked_for_write, 0);
            (void)InterlockedExchange(&clds_sorted_list->pending_write_operations, 0);

            /* Codes_SRS_CLDS_SORTED_LIST_01_058: [ start_sequence_number shall be used by the sorted list to compute the sequence number of each operation. ]*/
            clds_sorted_list->sequence_number = start_sequence_number;

            (void)InterlockedExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL);
        }
    }

    return clds_sorted_list;
}

void clds_sorted_list_destroy(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    if (clds_sorted_list == NULL)
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_005: [ If clds_sorted_list is NULL, clds_sorted_list_destroy shall return. ]*/
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p", clds_sorted_list);
    }
    else
    {
        CLDS_SORTED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL, NULL);

        /* Codes_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
        // go through all the items and free them
        while (current_item != NULL)
        {
            CLDS_SORTED_LIST_ITEM* next_item = (CLDS_SORTED_LIST_ITEM*)((uintptr_t)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL) & ~0x1);

            /* Codes_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback item_cleanup_callback passed to clds_sorted_list_node_create shall be called, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
            /* Codes_SRS_CLDS_SORTED_LIST_01_041: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the freed items. ]*/
            internal_node_destroy(current_item);
            current_item = next_item;
        }

        /* Codes_SRS_CLDS_SORTED_LIST_01_004: [ clds_sorted_list_destroy shall free all resources associated with the sorted list instance. ]*/
        free(clds_sorted_list);
    }
}

CLDS_SORTED_LIST_INSERT_RESULT clds_sorted_list_insert(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_INSERT_RESULT result;

    if (
        /* Codes_SRS_CLDS_SORTED_LIST_01_011: [ If clds_sorted_list is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_012: [ If item is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_013: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_062: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
        ((sequence_number != NULL) && (clds_sorted_list->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: clds_sorted_list = %p, item = %p, clds_hazard_pointers_thread = %p, sequence_number = %p",
            clds_sorted_list, item, clds_hazard_pointers_thread, sequence_number);
        result = CLDS_SORTED_LIST_INSERT_ERROR;
    }
    else
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_001: [ clds_sorted_list_insert shall try the following until it acquires a write lock for the list: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_002: [ clds_sorted_list_insert shall increment the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_003: [ If the counter to lock the list for writes is non-zero then: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_004: [ clds_sorted_list_insert shall decrement the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_005: [ clds_sorted_list_insert shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_sorted_list);

        bool restart_needed;
        void* new_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, item);

        /* Codes_SRS_CLDS_SORTED_LIST_01_069: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
        if (clds_sorted_list->sequence_number != NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_060: [ For each insert the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
            item->seq_no = InterlockedIncrement64(clds_sorted_list->sequence_number);

            /* Codes_SRS_CLDS_SORTED_LIST_01_061: [ If the sequence_number argument passed to clds_sorted_list_insert is NULL, the computed sequence number for the insert shall still be computed but it shall not be provided to the user. ]*/
            if (sequence_number != NULL)
            {
                *sequence_number = item->seq_no;
            }
        }

        /* Codes_SRS_CLDS_SORTED_LIST_01_047: [ clds_sorted_list_insert shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;
            result = CLDS_SORTED_LIST_INSERT_ERROR;

            do
            {
                // get the current_item value
                volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);

                // check if the item is NULL
                if ((void*)((uintptr_t)current_item & ~0x1) == NULL)
                {
                    item->next = NULL;

                    // not found, so insert it here
                    if (previous_item != NULL)
                    {
                        // have a previous item, try to replace the NULL with the new item
                        // if there is something else than NULL there, restart, there were some major changes
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)(uintptr_t)item, (PVOID)((uintptr_t)current_item & ~0x1)) != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
                            result = CLDS_SORTED_LIST_INSERT_OK;
                            break;
                        }
                    }
                    else
                    {
                        // no previous item, replace the head, make sure it is a "clean" NULL, no lock bit set
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, item, NULL) != NULL)
                        {
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            // insert done
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
                            result = CLDS_SORTED_LIST_INSERT_OK;
                            break;
                        }
                    }

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

                        if (clds_sorted_list->skipped_seq_no_cb != NULL)
                        {
                            /* Codes_SRS_CLDS_SORTED_LIST_01_079: [** If sequence numbers are generated and a skipped sequence number callback was provided to clds_sorted_list_create, when the item is indicated as already existing, the generated sequence number shall be indicated as skipped. ]*/
                            clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, item->seq_no);
                        }

                        LogError("Cannot acquire hazard pointer");
                        restart_needed = false;
                        result = CLDS_SORTED_LIST_INSERT_ERROR;
                        break;
                    }
                    else
                    {
                        // now make sure the item has not changed. This also takes care of checking that the delete lock bit is not set
                        if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL) != (PVOID)((uintptr_t)current_item & ~0x1))
                        {
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                            if (clds_sorted_list->skipped_seq_no_cb != NULL)
                            {
                                /* Codes_SRS_CLDS_SORTED_LIST_01_079: [** If sequence numbers are generated and a skipped sequence number callback was provided to clds_sorted_list_create, when the item is indicated as already existing, the generated sequence number shall be indicated as skipped. ]*/
                                clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, item->seq_no);
                            }

                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            // clear any leftover delete lock bit, since we made sure it is not set
                            current_item = (PVOID)((uintptr_t)current_item & ~0x1);

                            // we are in a stable state, at this point the previous node does not have a delete lock bit set
                            // compare the current item key to our key
                            void* current_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, (struct CLDS_SORTED_LIST_ITEM_TAG*)current_item);
                            int compare_result = clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, new_item_key, current_item_key);

                            if (compare_result == 0)
                            {
                                // item already in the list
                                if (previous_item != NULL)
                                {
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                restart_needed = false;

                                if (clds_sorted_list->skipped_seq_no_cb != NULL)
                                {
                                    /* Codes_SRS_CLDS_SORTED_LIST_01_079: [** If sequence numbers are generated and a skipped sequence number callback was provided to clds_sorted_list_create, when the item is indicated as already existing, the generated sequence number shall be indicated as skipped. ]*/
                                    clds_sorted_list->skipped_seq_no_cb(clds_sorted_list->skipped_seq_no_cb_context, item->seq_no);
                                }

                                /* Codes_SRS_CLDS_SORTED_LIST_01_048: [ If the item with the given key already exists in the list, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS. ]*/
                                result = CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS;
                                break;
                            }
                            else if (compare_result < 0)
                            {
                                // need to insert between the previous and current node, since current node's key is higher than what we want to insert
                                item->next = current_item;

                                if (previous_item != NULL)
                                {
                                    // have a previous item
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)item, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // let go of both hazard pointers
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // let go of both hazard pointers
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return 0. ]*/
                                        result = CLDS_SORTED_LIST_INSERT_OK;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)item, (PVOID)current_item) != current_item)
                                    {
                                        // let go of the hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // let go of the hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
                                        result = CLDS_SORTED_LIST_INSERT_OK;
                                        break;
                                    }
                                }
                            }
                            else // item is less than the current, so move on
                            {
                                // we have a stable pointer to the current item, now simply set the previous to be this
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                previous_hp = current_item_hp;
                                previous_item = current_item;
                                current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);

        /*Codes_SRS_CLDS_SORTED_LIST_42_051: [ clds_sorted_list_insert shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_sorted_list);
    }

    return result;
}

CLDS_SORTED_LIST_DELETE_RESULT clds_sorted_list_delete_item(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_DELETE_RESULT result;

    if (
        /* Codes_SRS_CLDS_SORTED_LIST_01_015: [ If clds_sorted_list is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_016: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_017: [ If item is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_065: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_delete shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_sorted_list->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, CLDS_SORTED_LIST_ITEM* item=%p, int64_t* sequence_number=%p",
            clds_sorted_list, clds_hazard_pointers_thread, item, sequence_number);
        result = CLDS_SORTED_LIST_DELETE_ERROR;
    }
    else
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_006: [ clds_sorted_list_delete_item shall try the following until it acquires a write lock for the list: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_007: [ clds_sorted_list_delete_item shall increment the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_008: [ If the counter to lock the list for writes is non-zero then: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_009: [ clds_sorted_list_delete_item shall decrement the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_010: [ clds_sorted_list_delete_item shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_sorted_list);

        /* Codes_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
        result = internal_delete(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_ptr, item, sequence_number);

        /*Codes_SRS_CLDS_SORTED_LIST_42_011: [ clds_sorted_list_delete_item shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_sorted_list);
    }

    return result;
}

CLDS_SORTED_LIST_DELETE_RESULT clds_sorted_list_delete_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_DELETE_RESULT result;

    if (
        /* Codes_SRS_CLDS_SORTED_LIST_01_020: [ If clds_sorted_list is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_021: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_022: [ If key is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_068: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_sorted_list->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, int64_t* sequence_number=%p",
            clds_sorted_list, clds_hazard_pointers_thread, key, sequence_number);
        result = CLDS_SORTED_LIST_DELETE_ERROR;
    }
    else
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_012: [ clds_sorted_list_delete_key shall try the following until it acquires a write lock for the list: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_013: [ clds_sorted_list_delete_key shall increment the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_014: [ If the counter to lock the list for writes is non-zero then: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_015: [ clds_sorted_list_delete_key shall decrement the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_016: [ clds_sorted_list_delete_key shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_sorted_list);

        /* Codes_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
        result = internal_delete(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_key, key, sequence_number);

        /*Codes_SRS_CLDS_SORTED_LIST_42_017: [ clds_sorted_list_delete_key shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_sorted_list);
    }

    return result;
}

CLDS_SORTED_LIST_REMOVE_RESULT clds_sorted_list_remove_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_SORTED_LIST_ITEM** item, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_REMOVE_RESULT result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_053: [ If clds_sorted_list is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
    if (
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_055: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_056: [ If key is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_075: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_sorted_list->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p, CLDS_SORTED_LIST_ITEM** item=%p, int64_t* sequence_number=%p",
            clds_sorted_list, clds_hazard_pointers_thread, key, item, sequence_number);
        result = CLDS_SORTED_LIST_REMOVE_ERROR;
    }
    else
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_018: [ clds_sorted_list_remove_key shall try the following until it acquires a write lock for the list: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_019: [ clds_sorted_list_remove_key shall increment the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_020: [ If the counter to lock the list for writes is non-zero then: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_021: [ clds_sorted_list_remove_key shall decrement the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_022: [ clds_sorted_list_remove_key shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_sorted_list);

        /* Codes_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
        result = internal_remove(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_key, key, item, sequence_number);

        /*Codes_SRS_CLDS_SORTED_LIST_42_023: [ clds_sorted_list_remove_key shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_sorted_list);
    }

    return result;
}

CLDS_SORTED_LIST_ITEM* clds_sorted_list_find_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_SORTED_LIST_ITEM* result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_028: [ If clds_sorted_list is NULL, clds_sorted_list_find shall fail and return NULL. ]*/
    if (
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_030: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_find shall fail and return NULL. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_031: [ If key is NULL, clds_sorted_list_find shall fail and return NULL. ]*/
        (key == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* key=%p",
            clds_sorted_list, clds_hazard_pointers_thread, key);
        result = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_027: [ clds_sorted_list_find shall find in the list the first item that matches the criteria given by a user compare function. ]*/

        bool restart_needed;
        result = NULL;

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

            do
            {
                // get the current_item value
                volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
                if (current_item == NULL)
                {
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    restart_needed = false;

                    /* Codes_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_sorted_list_find_key shall fail and return NULL. ]*/
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

                        /* Codes_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_sorted_list_find_key shall fail and return NULL. ]*/
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
                            void* item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, (struct CLDS_SORTED_LIST_ITEM_TAG*)current_item);
                            int compare_result = clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, key, item_key);
                            if (compare_result == 0)
                            {
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                // found it
                                /* Codes_SRS_CLDS_SORTED_LIST_01_034: [ clds_sorted_list_find_key shall return a pointer to the item with the reference count already incremented so that it can be safely used by the caller. ]*/
                                (void)InterlockedIncrement(&current_item->ref_count);
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                /* Codes_SRS_CLDS_SORTED_LIST_01_029: [ On success clds_sorted_list_find shall return a non-NULL pointer to the found linked list item. ]*/
                                result = (CLDS_SORTED_LIST_ITEM*)current_item;
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
                                current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);
    }

    return result;
}

CLDS_SORTED_LIST_SET_VALUE_RESULT clds_sorted_list_set_value(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, const void* key, CLDS_SORTED_LIST_ITEM* new_item, CLDS_SORTED_LIST_ITEM** old_item, int64_t* sequence_number)
{
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;

    if (
        /* Codes_SRS_CLDS_SORTED_LIST_01_081: [ If clds_sorted_list is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_082: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_083: [ If key is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        (key == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_084: [ If new_item is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        (new_item == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_085: [ If old_item is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        (old_item == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_086: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
        ((sequence_number != NULL) && (clds_sorted_list->sequence_number == NULL))
        )
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, const void* key=%p, CLDS_SORTED_LIST_ITEM* new_item=%p, CLDS_SORTED_LIST_ITEM** old_item=%p, int64_t* sequence_number=%p",
            clds_sorted_list, clds_hazard_pointers_thread, key, new_item, old_item, sequence_number);
        result = CLDS_SORTED_LIST_SET_VALUE_ERROR;
    }
    else
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_024: [ clds_sorted_list_set_value shall try the following until it acquires a write lock for the list: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_025: [ clds_sorted_list_set_value shall increment the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_026: [ If the counter to lock the list for writes is non-zero then: ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_027: [ clds_sorted_list_set_value shall decrement the count of pending write operations. ]*/
        /*Codes_SRS_CLDS_SORTED_LIST_42_028: [ clds_sorted_list_set_value shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
        check_lock_and_begin_write_operation(clds_sorted_list);

        bool restart_needed;
        void* new_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, new_item);

        /* Codes_SRS_CLDS_SORTED_LIST_01_091: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
        if (clds_sorted_list->sequence_number != NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_090: [ For each set value the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
            new_item->seq_no = InterlockedIncrement64(clds_sorted_list->sequence_number);

            /* Codes_SRS_CLDS_SORTED_LIST_01_092: [ If the sequence_number argument passed to clds_sorted_list_set_value is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. ]*/
            if (sequence_number != NULL)
            {
                *sequence_number = new_item->seq_no;
            }
        }

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;
            result = CLDS_SORTED_LIST_SET_VALUE_ERROR;

            do
            {
                // get the current_item value
                volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
                if (current_item == NULL)
                {
                    new_item->next = NULL;

                    // not found, so insert it here
                    if (previous_item != NULL)
                    {
                        // have a previous item
                        /* Codes_SRS_CLDS_SORTED_LIST_01_087: [ If the key entry does not exist in the list, new_item shall be inserted at the key position. ]*/
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)new_item, NULL) != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                            *old_item = NULL;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                            result = CLDS_SORTED_LIST_SET_VALUE_OK;
                            break;
                        }
                    }
                    else
                    {
                        // no previous item, replace the head
                        /* Codes_SRS_CLDS_SORTED_LIST_01_087: [ If the key entry does not exist in the list, new_item shall be inserted at the key position. ]*/
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, new_item, NULL) != NULL)
                        {
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            // insert done
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                            *old_item = NULL;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                            result = CLDS_SORTED_LIST_SET_VALUE_OK;
                            break;
                        }
                    }

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
                        result = CLDS_SORTED_LIST_SET_VALUE_ERROR;
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
                            // we are in a stable state, compare the current item key to our key
                            void* current_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, (struct CLDS_SORTED_LIST_ITEM_TAG*)current_item);
                            int compare_result = clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, new_item_key, current_item_key);

                            if (compare_result == 0)
                            {
                                /* Codes_SRS_CLDS_SORTED_LIST_01_088: [ If the key entry exists in the list, its value shall be replaced with new_item. ]*/

                                if (current_item == new_item)
                                {
                                    if (previous_item != NULL)
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                    }

                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                    restart_needed = false;

                                    /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                                    *old_item = (CLDS_SORTED_LIST_ITEM*)(void*)((uintptr_t)current_item & ~0x1);
                                    (void)clds_sorted_list_node_inc_ref(*old_item);

                                    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                    /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                                    result = CLDS_SORTED_LIST_SET_VALUE_OK;
                                    break;
                                }

                                // set the new_item->next to point to the next item in the list
                                new_item->next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);
                                if ((((uintptr_t)new_item->next) & 0x1) != 0)
                                {
                                    if (previous_item != NULL)
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                    }

                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                    restart_needed = true;
                                    break;
                                }

                                if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)new_item->next | 0x1), (PVOID)new_item->next) != (PVOID)new_item->next)
                                {
                                    if (previous_item != NULL)
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                    }

                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                    restart_needed = true;
                                    break;
                                }

                                // now its locked, no insert/delete should be happening on current->next
                                if (previous_item != NULL)
                                {
                                    // have a previous item
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)new_item, (PVOID)current_item) != current_item)
                                    {
                                        if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)new_item->next & 0x1), (PVOID)((uintptr_t)new_item->next | 0x1)) != (PVOID)((uintptr_t)new_item->next | 0x1))
                                        {
                                            LogError("This should not happen");
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                            restart_needed = false;
                                            result = CLDS_SORTED_LIST_SET_VALUE_ERROR;
                                            break;
                                        }
                                        else
                                        {
                                            // let go of previous hazard pointer
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                            restart_needed = true;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                                        *old_item = (CLDS_SORTED_LIST_ITEM*)(void*)((uintptr_t)current_item & ~0x1);
                                        (void)clds_sorted_list_node_inc_ref(*old_item);

                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                                        result = CLDS_SORTED_LIST_SET_VALUE_OK;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)new_item, (PVOID)current_item) != current_item)
                                    {
                                        if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)new_item->next & 0x1), (PVOID)((uintptr_t)new_item->next | 0x1)) != (PVOID)((uintptr_t)new_item->next | 0x1))
                                        {
                                            LogError("This should not happen");
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                            restart_needed = false;
                                            result = CLDS_SORTED_LIST_SET_VALUE_ERROR;
                                            break;
                                        }
                                        else
                                        {
                                            // let go of previous hazard pointer
                                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                            restart_needed = true;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                                        *old_item = (CLDS_SORTED_LIST_ITEM*)(void*)((uintptr_t)current_item & ~0x1);
                                        (void)clds_sorted_list_node_inc_ref(*old_item);

                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                                        result = CLDS_SORTED_LIST_SET_VALUE_OK;
                                        break;
                                    }

                                }

                                break;
                            }
                            else if (compare_result < 0)
                            {
                                // need to insert between these 2 nodes
                                new_item->next = current_item;

                                if (previous_item != NULL)
                                {
                                    // have a previous item
                                    /* Codes_SRS_CLDS_SORTED_LIST_01_087: [ If the key entry does not exist in the list, new_item shall be inserted at the key position. ]*/
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)new_item, (PVOID)current_item) != current_item)
                                    {
                                        // let go of previous hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                                        *old_item = NULL;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                                        result = CLDS_SORTED_LIST_SET_VALUE_OK;
                                        break;
                                    }
                                }
                                else
                                {
                                    /* Codes_SRS_CLDS_SORTED_LIST_01_087: [ If the key entry does not exist in the list, new_item shall be inserted at the key position. ]*/
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)new_item, (PVOID)current_item) != current_item)
                                    {
                                        // let go of previous hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
                                        *old_item = NULL;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
                                        result = CLDS_SORTED_LIST_SET_VALUE_OK;
                                        break;
                                    }
                                }
                            }
                            else // item is less than the current, so move on
                            {
                                // we have a stable pointer to the current item, now simply set the previous to be this
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                previous_hp = current_item_hp;
                                previous_item = current_item;
                                current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);

        /*Codes_SRS_CLDS_SORTED_LIST_42_029: [ clds_sorted_list_set_value shall decrement the count of pending write operations. ]*/
        end_write_operation(clds_sorted_list);
    }

    return result;
}

void clds_sorted_list_lock_writes(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    if (clds_sorted_list == NULL)
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_030: [ If clds_sorted_list is NULL then clds_sorted_list_lock_writes shall return. ]*/
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p", clds_sorted_list);
    }
    else
    {
        internal_lock_writes(clds_sorted_list);
    }
}

void clds_sorted_list_unlock_writes(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    if (clds_sorted_list == NULL)
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_033: [ If clds_sorted_list is NULL then clds_sorted_list_unlock_writes shall return. ]*/
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p", clds_sorted_list);
    }
    else
    {
        internal_unlock_writes(clds_sorted_list);
    }
}

CLDS_SORTED_LIST_GET_COUNT_RESULT clds_sorted_list_get_count(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, uint64_t* item_count)
{
    CLDS_SORTED_LIST_GET_COUNT_RESULT result;

    if (
        /*Codes_SRS_CLDS_SORTED_LIST_42_035: [ If clds_sorted_list is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /*Codes_SRS_CLDS_SORTED_LIST_42_036: [ If clds_hazard_pointers_thread is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /*Codes_SRS_CLDS_SORTED_LIST_42_037: [ If item_count is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
        (item_count == NULL)
        )
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_038: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED. ]*/
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, uint64_t* item_count=%p",
            clds_sorted_list, clds_hazard_pointers_thread, item_count);
        result = CLDS_SORTED_LIST_GET_COUNT_ERROR;
    }
    else
    {
        if (InterlockedAdd(&clds_sorted_list->locked_for_write, 0) == 0)
        {
            /*Codes_SRS_CLDS_SORTED_LIST_42_038: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED. ]*/
            LogError("Must lock the list before getting the count of items");
            result = CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED;
        }
        else
        {
            /*Codes_SRS_CLDS_SORTED_LIST_42_039: [ clds_sorted_list_get_count shall iterate over the items in the list and count them in item_count. ]*/
            uint64_t count = 0;
            CLDS_SORTED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL, NULL);

            while (current_item != NULL)
            {
                count++;
                CLDS_SORTED_LIST_ITEM* next_item = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);
                current_item = next_item;
            }

            *item_count = count;

            /*Codes_SRS_CLDS_SORTED_LIST_42_040: [ clds_sorted_list_get_count shall succeed and return CLDS_SORTED_LIST_GET_COUNT_OK. ]*/
            result = CLDS_SORTED_LIST_GET_COUNT_OK;
        }
    }

    return result;
}

CLDS_SORTED_LIST_GET_ALL_RESULT clds_sorted_list_get_all(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, uint64_t item_count, CLDS_SORTED_LIST_ITEM** items)
{
    CLDS_SORTED_LIST_GET_ALL_RESULT result;

    if (
        /*Codes_SRS_CLDS_SORTED_LIST_42_041: [ If clds_sorted_list is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
        (clds_sorted_list == NULL) ||
        /*Codes_SRS_CLDS_SORTED_LIST_42_042: [ If clds_hazard_pointers_thread is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /*Codes_SRS_CLDS_SORTED_LIST_42_043: [ If item_count is 0 then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
        (item_count == 0) ||
        /*Codes_SRS_CLDS_SORTED_LIST_42_044: [ If items is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
        (items == NULL)
        )
    {
        /*Codes_SRS_CLDS_SORTED_LIST_42_045: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED. ]*/
        LogError("Invalid arguments: CLDS_SORTED_LIST_HANDLE clds_sorted_list=%p, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, uint64_t item_count=%" PRIu64 ", CLDS_SORTED_LIST_ITEM* items=%p",
            clds_sorted_list, clds_hazard_pointers_thread, item_count, items);
        result = CLDS_SORTED_LIST_GET_ALL_ERROR;
    }
    else
    {
        if (InterlockedAdd(&clds_sorted_list->locked_for_write, 0) == 0)
        {
            /*Codes_SRS_CLDS_SORTED_LIST_42_045: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED. ]*/
            LogError("Must lock the list before getting all items");
            result = CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED;
        }
        else
        {
            result = CLDS_SORTED_LIST_GET_ALL_OK;

            /*Codes_SRS_CLDS_SORTED_LIST_42_046: [ For each item in the list: ]*/
            uint64_t current_index = 0;
            CLDS_SORTED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL, NULL);

            while (current_item != NULL)
            {
                CLDS_SORTED_LIST_ITEM* next_item = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                if (current_index + 1 > item_count)
                {
                    /*Codes_SRS_CLDS_SORTED_LIST_42_049: [ If item_count does not match the number of items in the list then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE. ]*/
                    LogError("Attempted to get all items with array of size %" PRIu64 ", but there were at least %" PRIu64 " items in the list (too small)",
                        item_count, current_index + 1);
                    result = CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE;
                    break;
                }

                /*Codes_SRS_CLDS_SORTED_LIST_42_047: [ clds_sorted_list_get_all shall increment the ref count. ]*/
                (void)clds_sorted_list_node_inc_ref(current_item);

                /*Codes_SRS_CLDS_SORTED_LIST_42_048: [ clds_sorted_list_get_all shall store the pointer in items. ]*/
                items[current_index] = current_item;

                current_index++;
                current_item = next_item;
            }

            if (current_item != NULL || current_index != item_count)
            {
                if (current_index < item_count)
                {
                    /*Codes_SRS_CLDS_SORTED_LIST_42_049: [ If item_count does not match the number of items in the list then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE. ]*/
                    LogError("Attempted to get all items with array of size %" PRIu64 ", but there were %" PRIu64 " items in the list (too large)",
                        item_count, current_index);
                    result = CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE;
                }

                for (uint64_t i = 0; i < current_index; ++i)
                {
                    clds_sorted_list_node_release(items[i]);
                    items[i] = NULL;
                }
            }
            else
            {
                /*Codes_SRS_CLDS_SORTED_LIST_42_050: [ clds_sorted_list_get_all shall succeed and return CLDS_SORTED_LIST_GET_ALL_OK. ]*/
                result = CLDS_SORTED_LIST_GET_ALL_OK;
            }
        }
    }

    return result;
}

CLDS_SORTED_LIST_ITEM* clds_sorted_list_node_create(size_t node_size, SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    /* Codes_SRS_CLDS_SORTED_LIST_01_036: [ item_cleanup_callback shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_037: [ item_cleanup_callback_context shall be allowed to be NULL. ]*/
    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("malloc failed");
    }
    else
    {
        volatile CLDS_SORTED_LIST_ITEM* item = (volatile CLDS_SORTED_LIST_ITEM*)((unsigned char*)result);
        item->item_cleanup_callback = item_cleanup_callback;
        item->item_cleanup_callback_context = item_cleanup_callback_context;
        (void)InterlockedExchange(&item->ref_count, 1);
        (void)InterlockedExchangePointer((volatile PVOID*)&item->next, NULL);
    }

    return result;
}

int clds_sorted_list_node_inc_ref(CLDS_SORTED_LIST_ITEM* item)
{
    int result;

    if (item == NULL)
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_ITEM* item=%p", item);
        result = MU_FAILURE;
    }
    else
    {
        (void)InterlockedIncrement(&item->ref_count);
        result = 0;
    }

    return result;
}

void clds_sorted_list_node_release(CLDS_SORTED_LIST_ITEM* item)
{
    if (item == NULL)
    {
        LogError("Invalid arguments: CLDS_SORTED_LIST_ITEM* item=%p", item);
    }
    else
    {
        internal_node_destroy(item);
    }
}
