// Copyright (C) Microsoft Corporation. All rights reserved.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "azure_c_util/gballoc.h"
#include "azure_c_util/xlogging.h"
#include "clds/lock_free_set.h"
#include "clds/clds_atomics.h"

/* this is a lock free set that is backed by a doubly linked list */

typedef struct LOCK_FREE_SET_TAG
{
    volatile CLDS_ATOMIC(intptr_t) head;
} LOCK_FREE_SET;

static void internal_purge_not_thread_safe(LOCK_FREE_SET_HANDLE lock_free_set, NODE_CLEANUP_FUNC node_cleanup_callback, void* context)
{
    /* Codes_SRS_LOCK_FREE_SET_01_023: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_purge_not_thread_safe is called. ]*/
    /* Codes_SRS_LOCK_FREE_SET_01_007: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_destroy is called. ]*/
    if (node_cleanup_callback != NULL)
    {
        LOCK_FREE_SET_ITEM* current_item = (LOCK_FREE_SET_ITEM*)(void*)lock_free_set->head;

        // go through each of the nodes that are left and call the cleanup callback for it
        // no need to be thread safe
        while (current_item != NULL)
        {
            // do first the other nodes
            LOCK_FREE_SET_ITEM* cleanup_item = current_item;
            current_item = (LOCK_FREE_SET_ITEM*)current_item->next;

            /* Codes_SRS_LOCK_FREE_SET_01_024: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
            /* Codes_SRS_LOCK_FREE_SET_01_008: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
            node_cleanup_callback(context, cleanup_item);
        }
    }

    lock_free_set->head = (CLDS_ATOMIC(intptr_t))NULL;
}

LOCK_FREE_SET_HANDLE lock_free_set_create(void)
{
    /* Codes_SRS_LOCK_FREE_SET_01_001: [ lock_free_set_create shall create a lock free set. ] */
    LOCK_FREE_SET_HANDLE lock_free_set = malloc(sizeof(LOCK_FREE_SET));
    if (lock_free_set == NULL)
    {
        /* Codes_SRS_LOCK_FREE_SET_01_003: [ If lock_free_set_create fails allocating memory for the set then it shall fail and return NULL. ] */
        LogError("malloc failed");
    }
    else
    {
        /* Codes_SRS_LOCK_FREE_SET_01_002: [ On success, lock_free_set_create shall return a non-NULL handle to the newly created set. ]*/
        lock_free_set->head = (CLDS_ATOMIC(intptr_t))NULL;
    }

    return lock_free_set;
}

void lock_free_set_destroy(LOCK_FREE_SET_HANDLE lock_free_set, NODE_CLEANUP_FUNC node_cleanup_callback, void* context)
{
    /* Codes_SRS_LOCK_FREE_SET_01_006: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
    if (lock_free_set == NULL)
    {
        /* Codes_SRS_LOCK_FREE_SET_01_005: [ If lock_free_set is NULL, lock_free_set_destroy shall do nothing. ]*/
        LogError("NULL lock_free_set");
    }
    else
    {
        internal_purge_not_thread_safe(lock_free_set, node_cleanup_callback, context);

        /* Codes_SRS_LOCK_FREE_SET_01_004: [ lock_free_set_destroy shall free all resources associated with the lock free set. ]*/
        free(lock_free_set);
    }
}

int lock_free_set_insert(LOCK_FREE_SET_HANDLE lock_free_set, LOCK_FREE_SET_ITEM* item)
{
    int result;

    if (
        /* Codes_SRS_LOCK_FREE_SET_01_011: [ If lock_free_set or item is NULL, lock_free_set_insert shall fail and return a non-zero value. ]*/
        (lock_free_set == NULL) ||
        (item == NULL)
        )
    {
        LogError("Invalid arguments: LOCK_FREE_SET_HANDLE lock_free_set=%p, LOCK_FREE_SET_ITEM* item=%p",
            lock_free_set, item);
        result = MU_FAILURE;
    }
    else
    {
        bool restart_needed;
        result = MU_FAILURE;

        /* Codes_SRS_LOCK_FREE_SET_01_013: [ lock_free_set_insert and lock_free_set_remove shall be safe to be called from multiple threads. ]*/

        /* Codes_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
        do
        {
            LOCK_FREE_SET_ITEM* current_head = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head);

            item->previous = (CLDS_ATOMIC(intptr_t))NULL;
            item->next = (CLDS_ATOMIC(intptr_t))current_head;

            // insert our new item as the head
            LOCK_FREE_SET_ITEM* expected_head = current_head;
            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_head, (CLDS_ATOMIC(intptr_t))(void*)item))
            {
                // unable to lock the head, it has already changed, restart
                restart_needed = true;
            }
            else
            {
                // replaced it, yay
                if (current_head == NULL)
                {
                    // the list was empty, we have nothing else to do
                    /* Codes_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
                    restart_needed = false;
                    result = 0;
                }
                else
                {
                    // Now we have
                    //         item              current_head          next_item
                    // [NULL, current_head]    [NULL, next item]    [ current_head, ...]

                    // we have to exchange the previous of "current_head" with our new item that just became the head
                    // Possible concurrent operations:
                    // - insert - any insert that attempts to change the head would fail and restart as it would notice the value of the head changed
                    // - a remove that tried to remove cuurent_head - a remove that tried to delete "current_head" would see that the head has changed in the meanwhile and it would need to restart
                    // - remove that removed from the middle - a remove that tried to delete "current_head" from the middle would see NULL, which is not OK for removing from the middle, thus it would restart

                    // Conclusion: current_head can not be deleted until we complete if we got far enough to change the head of the list

                    bool wait_for_next_node;
                    restart_needed = true;

                    do
                    {
                        LOCK_FREE_SET_ITEM* expected_current_head_previous = NULL;
                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&current_head->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_current_head_previous, (CLDS_ATOMIC(intptr_t))(void*)item))
                        {
                            // if the current_head->previous is not NULL, it can only mean that a remove is in progress, we'll wait
                            wait_for_next_node = true;
                        }
                        else
                        {
                            /* Codes_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
                            wait_for_next_node = false;
                            restart_needed = false;
                            result = 0;
                        }
                    } while (wait_for_next_node);
                }
            }
        } while (restart_needed);
    }

    return result;
}

int lock_free_set_remove(LOCK_FREE_SET_HANDLE lock_free_set, LOCK_FREE_SET_ITEM* item)
{
    int result;

    if (
        /* Codes_SRS_LOCK_FREE_SET_01_017: [ If lock_free_set or item is NULL, lock_free_set_remove shall fail and return a non-zero value. ]*/
        (lock_free_set == NULL) ||
        (item == NULL)
        )
    {
        LogError("Invalid arguments: LOCK_FREE_SET_HANDLE lock_free_set=%p, LOCK_FREE_SET_ITEM* item=%p",
            lock_free_set, item);
        result = MU_FAILURE;
    }
    else
    {
        bool restart_needed;

        /* Codes_SRS_LOCK_FREE_SET_01_013: [ lock_free_set_insert and lock_free_set_remove shall be safe to be called from multiple threads. ]*/

        // Code Analysis is rather upset if we do not do that, but for no reason :-(
        result = MU_FAILURE;

        /* Codes_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/

        // we have to traverse the tree to get the address of the item
        do
        {
            // first get the head so that we know whether we are removing the head
            LOCK_FREE_SET_ITEM* current_head = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head);

            if (current_head == item)
            {
                // we locked the left side, a node that tries to insert would wait because it would see a non NULL value, so that means that the node to the left will not be able to be deleted immediately
                // we are removing the head. we can definitely update the memory for our current item, so we will mark the next link as to be removed, so that any node that comes in will not try to change that
                LOCK_FREE_SET_ITEM* item_next = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next);
                if (item_next == NULL)
                {
                    //LogInfo("Delete head, NULL previous and next, TID=%lu", GetCurrentThreadId());

                    // mark the left link for deletion, even though it is NULL, just in case some other node inserts exactly at this time
                    LOCK_FREE_SET_ITEM* item_previous = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous);
                    if (item_previous != NULL)
                    {
                        // since we are the head, but the previous item is non NULL, we must wait as some other node is being removed at our left
                        restart_needed = true;
                    }
                    else
                    {
                        // now "lock" the previous node
                        LOCK_FREE_SET_ITEM* expected_item_previous = item_previous;
                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))((intptr_t)item_previous | 0x1)))
                        {
                            // we could not mark the next link as to be deleted because someone else came and modified it, this is most likely due to a node being inserted,
                            // so we must restart
                            restart_needed = true;
                        }
                        else
                        {
                            // there is no next node, we just have to NULL the head if it has not changed
                            LOCK_FREE_SET_ITEM* expected_head = item;
                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_head, (CLDS_ATOMIC(intptr_t))NULL))
                            {
                                // unlock the previous node
                                expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                {
                                    LogError("Unexpected change of locked item->previous");
                                    result = MU_FAILURE;
                                    restart_needed = false;
                                }
                                else
                                {
                                    // we could not mark the next link as to be deleted because someone else came and modified it, simply restart
                                    restart_needed = true;
                                }
                            }
                            else
                            {
                                /* Codes_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/

                                // head has not changed (or we had an ABA on it, but we don't care about that in this case!)
                                // we replaced the head with NULL, all happy
                                restart_needed = false;
                                result = 0;
                            }
                        }
                    }
                }
                else
                {
                    //LogInfo("Delete head, has next items, TID=%lu", GetCurrentThreadId());

                    // mark the left link for deletion, even though it is NULL
                    LOCK_FREE_SET_ITEM* item_previous = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous);
                    if (item_previous != NULL)
                    {
                        // since we are the head, but the previous item is non NULL, we must wait as some other node is being removed at our left
                        restart_needed = true;
                    }
                    else
                    {
                        // now "lock" the previous node
                        LOCK_FREE_SET_ITEM* expected_item_previous = item_previous;
                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))((intptr_t)item_previous | 0x1)))
                        {
                            // we could not mark the next link as to be deleted because someone else came and modified it, this is most likely due to a node being inserted,
                            // so we must restart
                            restart_needed = true;
                        }
                        else
                        {
                            // there is a next node, we need to mark the next link for deletion and proceed with changing the next node
                            LOCK_FREE_SET_ITEM* expected_item_next = item_next;
                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))((intptr_t)item_next | 0x1)))
                            {
                                // unlock the previous node
                                expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                {
                                    LogError("Unexpected change of locked item->previous");
                                    result = MU_FAILURE;
                                    restart_needed = false;
                                }
                                else
                                {
                                    // we could not mark the next link as to be deleted because someone else came and modified it, simply restart
                                    restart_needed = true;
                                }
                            }
                            else
                            {
                                // we marked the link for deletion, any next node will see that we're deleting and back off
                                // first thing, we need to change the head to point to the next item. This is because it is much harder to unwind changes
                                // if we would first change the item->next->previous
                                LOCK_FREE_SET_ITEM* expected_head = item;
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_head, (CLDS_ATOMIC(intptr_t))item_next))
                                {
                                    // head has changed
                                    // someone is inserting, we should not go further, as it is more likely that we will not be the head very soon
                                    // unlock previous and next link and restart
                                    expected_item_next = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_next | 0x1);
                                    if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))item_next))
                                    {
                                        LogError("Unexpected change of locked item->next");
                                        result = MU_FAILURE;
                                        restart_needed = false;
                                    }
                                    else
                                    {
                                        expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                        {
                                            LogError("Unexpected change of locked item->previous");
                                            result = MU_FAILURE;
                                            restart_needed = false;
                                        }
                                        else
                                        {
                                            restart_needed = true;
                                        }
                                    }
                                }
                                else
                                {
                                    // head has been changed, now we must update item->next->previous to NULL (which is the current head->previous)
                                    // while we do this several things could happen:
                                    // 1. the current head could be removed
                                    // it will see that the previous link is not NULL, it should back off in that case and restart
                                    // 2. another node could get inserted
                                    // that insert should see that there is a non-NULL previous link on the so called head and then just wait there

                                    bool wait_for_next_node;
                                    restart_needed = true;
                                    do
                                    {
                                        LOCK_FREE_SET_ITEM* expected_item_next_previous = (LOCK_FREE_SET_ITEM*)(void*)item;
                                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_next->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next_previous, (CLDS_ATOMIC(intptr_t))NULL))
                                        {
                                            // could not change the previous on the "current head", because the value has changed
                                            // It could have changed because:
                                            // 1. "current_head" being removed and thus left link being marked for deletion
                                            // In this case we should wait, as the node doing that will back off as we have priority and it will see our current item's next link marked for deletion
                                            wait_for_next_node = true;
                                        }
                                        else
                                        {
                                            // all OK, changed
                                            wait_for_next_node = false;
                                            restart_needed = false;
                                            result = 0;
                                        }
                                    } while (wait_for_next_node);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                // we are not the head node
                // we need to mark the previous node for deletion
                LOCK_FREE_SET_ITEM* item_previous = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous);
                if (item_previous == NULL)
                {
                    // somehow the previous value is now NULL, that means that by this time some other thread got to remove the node that was our previous, so it looks like
                    // now item is the head, thus restart to go on that path
                    restart_needed = true;
                }
                else
                {
                    LOCK_FREE_SET_ITEM* expected_item_previous = item_previous;
                    if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))((intptr_t)item_previous | 0x1)))
                    {
                        // item_previous has changed, we will need to restart, as we might be now the head of the list
                        restart_needed = true;
                    }
                    else
                    {
                        // we were able to mark the previous link for deletion
                        // we now want to mark the next link
                        // get the item->next value
                        LOCK_FREE_SET_ITEM* item_next = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next);
                        // If that is the case then the next node will back off when it will see
                        if (item_next == NULL)
                        {
                            //LogInfo("Delete tail, TID=%lu", GetCurrentThreadId());

                            // check if we somehow became head, if we did we need to bail out as the left node was removed already and we cannot compensate for that
                            LOCK_FREE_SET_ITEM* expected_item = item;
                            if (clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item, (CLDS_ATOMIC(intptr_t))((intptr_t)item)))
                            {
                                // we are the head node, restart
                                // we have to back off, as the previous node wins, unlock previous link
                                expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                {
                                    LogError("Unexpected change of locked item->previous");
                                    result = MU_FAILURE;
                                    restart_needed = false;
                                }
                                else
                                {
                                    // all unlock went fine, simply restart
                                    restart_needed = true;
                                }
                            }
                            else
                            {
                                // NULL next, we basically have to remove ourselfes from the tail
                                // Our layout now looks like
                                //
                                // [head]   ...   [previous]                [item]
                                // [NULL, ...]   [...,  item]    [{previous,del}, NULL]

                                // now change the previous->next to point to next (which is NULL)
                                LOCK_FREE_SET_ITEM* expected_item_previous_next = item;
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_previous->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous_next, (CLDS_ATOMIC(intptr_t))NULL))
                                {
                                    // item->previous->next changed, probably item->previous it is being deleted, we need to backoff since previous nodes have priority
                                    // by our convention

                                    // we simply unmark for deletion the previous link
                                    expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                    if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                    {
                                        // this is an error, someone modified the previous link and they should not
                                        LogError("item->previous modified unexpectedly");
                                        restart_needed = false;
                                        result = MU_FAILURE;
                                    }
                                    else
                                    {
                                        // we unmarked for deletion the previous link, we're in the clear to restart
                                        restart_needed = true;
                                    }
                                }
                                else
                                {
                                    /* Codes_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
                                    // we were able to change the item->previous->next to NULL, we are done removing
                                    restart_needed = false;
                                    result = 0;

                                    if ((((intptr_t)item->next & ~0x1) == 0) &&
                                        (((intptr_t)item->previous & ~0x1) == 0))
                                    {
                                        LogError("Anomaly");
                                    }
                                }
                            }
                        }
                        else
                        {
                            //LogInfo("Delete in middle, TID=%lu", GetCurrentThreadId());

                            // non NULL next
                            // mark the item->next for deletion
                            LOCK_FREE_SET_ITEM* expected_item_next = item_next;
                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))((intptr_t)item_next | 0x1)))
                            {
                                // could not mark the next link as to be removed because it changed, try again
                                expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                {
                                    // this is an error, someone modified the previous link and they should not
                                    LogError("item->previous modified unexpectedly");
                                    restart_needed = false;
                                    result = MU_FAILURE;
                                }
                                else
                                {
                                    // we unmarked for deletion the previous link, we're in the clear to restart
                                    restart_needed = true;
                                }
                            }
                            else
                            {
                                // check if we somehow became head, if we did we need to bail out as the left node was removed already and we cannot compensate for that
                                LOCK_FREE_SET_ITEM* expected_item = item;
                                if (clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&lock_free_set->head, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item, (CLDS_ATOMIC(intptr_t))((intptr_t)item)))
                                {
                                    // we are the head node, restart
                                    // we have to back off, as the previous node wins, unlock previous link
                                    expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                    if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                    {
                                        LogError("Unexpected change of locked item->previous");
                                        result = MU_FAILURE;
                                        restart_needed = false;
                                    }
                                    else
                                    {
                                        // unlock next link and restart
                                        expected_item_next = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_next | 0x1);
                                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))item_next))
                                        {
                                            LogError("Unexpected change of locked item->next");
                                            result = MU_FAILURE;
                                            restart_needed = false;
                                        }
                                        else
                                        {
                                            // all unlock went fine, simply restart
                                            restart_needed = true;
                                        }
                                    }
                                }
                                else
                                {
                                    // our next step is to update next to previous to point to next
                                    // that is because we should first do the work where we cannot win so that we do not have to undo anything
                                    restart_needed = true;

                                    // we got to mark that we're deleting the next link, so we can be sure that the right node if any won't be deleted
                                    // Our layout now looks like
                                    //
                                    // [head]   ...   [previous]                [item]                  [next]
                                    // [NULL, ...]   [...,  item]    [{previous,del}, {next,del}]    [item, ...]

                                    // now we attempt to change the previous->next to be next
                                    LOCK_FREE_SET_ITEM* item_previous_next = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_previous->next);
                                    if (((uintptr_t)item_previous_next & 0x1) != 0)
                                    {
                                        // the previous->next link is marked for deletion, this means that previous is being deleted
                                        // we have to back off, as the previous node wins, unlock previous link
                                        expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                        if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                        {
                                            LogError("Unexpected change of locked item->previous");
                                            result = MU_FAILURE;
                                            restart_needed = false;
                                        }
                                        else
                                        {
                                            // unlock next link and restart
                                            expected_item_next = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_next | 0x1);
                                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))item_next))
                                            {
                                                LogError("Unexpected change of locked item->next");
                                                result = MU_FAILURE;
                                                restart_needed = false;
                                            }
                                            else
                                            {
                                                // all unlock went fine, simply restart
                                                restart_needed = true;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (item_previous_next != item)
                                        {
                                            // we have to back off, as the previous node wins, unlock previous link
                                            expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                            {
                                                LogError("Unexpected change of locked item->previous");
                                                result = MU_FAILURE;
                                                restart_needed = false;
                                            }
                                            else
                                            {
                                                // unlock next link and restart
                                                expected_item_next = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_next | 0x1);
                                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))item_next))
                                                {
                                                    LogError("Unexpected change of locked item->next");
                                                    result = MU_FAILURE;
                                                    restart_needed = false;
                                                }
                                                else
                                                {
                                                    // all unlock went fine, simply restart
                                                    restart_needed = true;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            // previous->next is unlocked, change it
                                            // now change the previous->next to point to next
                                            LOCK_FREE_SET_ITEM* expected_item_previous_next = item;
                                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_previous->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous_next, (CLDS_ATOMIC(intptr_t))item_next))
                                            {
                                                // previous->next has changed (probably locked now), so
                                                // we have to back off, as the previous node wins, unlock previous link
                                                expected_item_previous = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_previous | 0x1);
                                                if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                                {
                                                    LogError("Unexpected change of locked item->previous");
                                                    result = MU_FAILURE;
                                                    restart_needed = false;
                                                }
                                                else
                                                {
                                                    // unlock next link and restart
                                                    expected_item_next = (LOCK_FREE_SET_ITEM*)(void*)((intptr_t)item_next | 0x1);
                                                    if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item->next, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next, (CLDS_ATOMIC(intptr_t))item_next))
                                                    {
                                                        LogError("Unexpected change of locked item->next");
                                                        result = MU_FAILURE;
                                                        restart_needed = false;
                                                    }
                                                    else
                                                    {
                                                        // all unlock went fine, simply restart
                                                        restart_needed = true;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                bool wait_for_next_node = false;

                                                // we successfully changed the item->previous->next value to whatever we had in item->next
                                                //
                                                // Our layout now looks like
                                                //
                                                // [head]   ...   [previous]                [item]                  [next]
                                                // [NULL, ...]   [...,  next]    [{previous,del}, {next,del}]    [item, ...]

                                                do
                                                {
                                                    // first get the next->previous and check if it is locked
                                                    LOCK_FREE_SET_ITEM* item_next_previous = (LOCK_FREE_SET_ITEM*)(void*)clds_atomic_load_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_next->previous);
                                                    if (((intptr_t)item_next_previous & 0x1) != 0)
                                                    {
                                                        // the next->previous is locked, we'll have to insist, since it has to unlock
                                                        wait_for_next_node = true;
                                                    }
                                                    else
                                                    {
                                                        // check if the next->previous matches what we'd expect
                                                        if (item_next_previous != item)
                                                        {
                                                            if (item_next_previous != NULL)
                                                            {
                                                                // oops, someone is removing at the right side
                                                                wait_for_next_node = true;
                                                            }
                                                            else
                                                            {
                                                                // NULL means that there were already a couple of operations happening and we don't need to update anymore the item->next->previous link
                                                                wait_for_next_node = false;
                                                                restart_needed = false;
                                                                /* Codes_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
                                                                result = 0;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            // the next->previous is not marked for deletion, we should proceed with our delete
                                                            LOCK_FREE_SET_ITEM* expected_item_next_previous = item_next_previous;
                                                            if (!clds_atomic_compare_exchange_strong_intptr_t((volatile CLDS_ATOMIC(intptr_t)*)(volatile void*)&item_next->previous, (CLDS_ATOMIC(intptr_t)*)(void*)&expected_item_next_previous, (CLDS_ATOMIC(intptr_t))item_previous))
                                                            {
                                                                // Clearly someone messed with the item->next->previous link, we shall not allow that
                                                                // our rule is that previous node wins, so in this case insist by retrying the set of item->next->previous
                                                                // the next node should back off as it sees that we are getting deleted
                                                                wait_for_next_node = true;
                                                            }
                                                            else
                                                            {
                                                                // we successfully changed the item->next->previous value to whatever we had in item->previous
                                                                //
                                                                // Our layout now looks like
                                                                //
                                                                // [head]   ...   [previous]                [item]                  [next]
                                                                // [NULL, ...]   [...,  next]    [{previous,del}, {next,del}]    [previous, ...]

                                                                // we are done
                                                                wait_for_next_node = false;
                                                                restart_needed = false;
                                                                /* Codes_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
                                                                result = 0;
                                                            }
                                                        }
                                                    }
                                                } while (wait_for_next_node);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } while (restart_needed);
    }

    return result;
}

int lock_free_set_purge_not_thread_safe(LOCK_FREE_SET_HANDLE lock_free_set, NODE_CLEANUP_FUNC node_cleanup_callback, void* context)
{
    int result;

    /* Codes_SRS_LOCK_FREE_SET_01_022: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
    if (lock_free_set == NULL)
    {
        /* Codes_SRS_LOCK_FREE_SET_01_021: [ If lock_free_set is NULL, lock_free_set_purge_not_thread_safe shall fail and return a non-zero value. ]*/
        LogError("Invalid arguments: LOCK_FREE_SET_HANDLE lock_free_set=%p, NODE_CLEANUP_FUNC node_cleanup_callback=%p, void* context=%p",
            lock_free_set, node_cleanup_callback, context);
        result = MU_FAILURE;
    }
    else
    {
        /* Codes_SRS_LOCK_FREE_SET_01_019: [ lock_free_set_purge_not_thread_safe shall remove all items in the set. ]*/
        internal_purge_not_thread_safe(lock_free_set, node_cleanup_callback, context);

        /* Codes_SRS_LOCK_FREE_SET_01_020: [ On success it shall return 0. ]*/
        result = 0;
    }

    return result;
}
