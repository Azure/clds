// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdbool.h>

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/interlocked.h"

#include "clds/mpsc_lock_free_queue.h"

/* this is a multi producer single consumer queue */

typedef struct MPSC_LOCK_FREE_QUEUE_TAG
{
    MPSC_LOCK_FREE_QUEUE_ITEM* volatile_atomic dequeue_head;
    MPSC_LOCK_FREE_QUEUE_ITEM* volatile_atomic enqueue_head;
} MPSC_LOCK_FREE_QUEUE;

MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue_create(void)
{
    /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_001: [ mpsc_lock_free_queue_create shall create a lock free multiple producer single consumer queue. ] */
    MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue = malloc(sizeof(MPSC_LOCK_FREE_QUEUE));
    if (mpsc_lock_free_queue == NULL)
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_003: [ If mpsc_lock_free_queue_create fails allocating memory for the queue then it shall fail and return NULL. ]*/
        LogError("Cannot create lock free queue");
    }
    else
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_002: [ On success, mpsc_lock_free_queue_create shall return a non-NULL handle to the newly created queue. ]*/
        mpsc_lock_free_queue->dequeue_head = NULL;
        mpsc_lock_free_queue->enqueue_head = NULL;
    }

    return mpsc_lock_free_queue;
}

void mpsc_lock_free_queue_destroy(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue)
{
    if (mpsc_lock_free_queue == NULL)
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_005: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_destroy shall do nothing. ]*/
        LogError("NULL mpsc_lock_free_queue");
    }
    else
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_004: [ mpsc_lock_free_queue_destroy shall free all resources associated with the lock free queue. ]*/
        free(mpsc_lock_free_queue);
    }
}

int mpsc_lock_free_queue_enqueue(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue, MPSC_LOCK_FREE_QUEUE_ITEM* item)
{
    int result;

    if ((mpsc_lock_free_queue == NULL) ||
        (item == NULL))
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_008: [ If mpsc_lock_free_queue or item is NULL, mpsc_lock_free_queue_enqueue shall fail and return a non-zero value. ]*/
        LogError("Bad arguments: mpsc_lock_free_queue = %p, item = %p",
            mpsc_lock_free_queue, item);
        result = MU_FAILURE;
    }
    else
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_006: [ mpsc_lock_free_queue_enqueue shall enqueue the item item in the queue. ]*/
        // enqueue simply puts the item in the enqueue_head

        // it is assumed that destroy is not called concurrently with enqueue

        do
        {
            /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_014: [** Concurrent mpsc_lock_free_queue_enqueue from multiple threads shall be safe. ]*/

            // first get the current enqueue_head
            MPSC_LOCK_FREE_QUEUE_ITEM* current_head = interlocked_compare_exchange_pointer((void* volatile_atomic*)&mpsc_lock_free_queue->enqueue_head, NULL, NULL);

            // now set the item's next to the head
            item->next = current_head;

            // now simply exchange the head if it has not changed in the meanwhile
            if (interlocked_compare_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->enqueue_head, item, current_head) == current_head)
            {
                // yay, we have inserted the item, simply bail out, we are done!
                break;
            }

            // unfortunatelly someone else tried to insert while we were replacing the head, so restart the insert
        } while (1);

        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_007: [ On success it shall return 0. ]*/
        result = 0;
    }

    return result;
}

MPSC_LOCK_FREE_QUEUE_ITEM* mpsc_lock_free_queue_dequeue(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue)
{
    MPSC_LOCK_FREE_QUEUE_ITEM* result;

    if (mpsc_lock_free_queue == NULL)
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_011: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_dequeue shall fail and return NULL. ]*/
        LogError("NULL mpsc_lock_free_queue");
        result = NULL;
    }
    else
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_009: [ mpsc_lock_free_queue_dequeue shall dequeue the oldest item in the queue. ]*/

        // it is assumed that destroy is not called concurrently with dequeue

        // there is only one consumer, so we're guaranteed by the semantics of the queue that noone else is removing
        // items. Items could be added at any rate but we do not care about that

        // first simply get the head of the dequeue list
        result = interlocked_compare_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, NULL, NULL);
        if (result != NULL)
        {
            // simply advance the dequeue head
            (void)interlocked_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, result->next);
        }
        else
        {
            // nothing in the dequeue list, so we have to move some items from the enqueue list
            // simply set the head to NULL and grab the entire list
            MPSC_LOCK_FREE_QUEUE_ITEM* temporary_list = interlocked_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->enqueue_head, NULL);

            if (temporary_list != NULL)
            {
                // now that the items are in the temporary list, we simply have to put them in reverse order in the dequeue list,
                // since in the dequeue list they will be pulled out LIFO.
                // enqueue order: 0, 1, 2
                // They ended up in the enqueue list being: 2 -> 1 -> 0, where 2 is the head.
                // We need to get them to be 0 -> 1 -> 2 in the dequeue list.

                MPSC_LOCK_FREE_QUEUE_ITEM* new_dequeue_head = NULL;
                MPSC_LOCK_FREE_QUEUE_ITEM* current_item = temporary_list;

                while (current_item != NULL)
                {
                    MPSC_LOCK_FREE_QUEUE_ITEM* next_item = current_item->next;
                    current_item->next = new_dequeue_head;
                    new_dequeue_head = current_item;
                    current_item = next_item;
                }

                // now we got in the dequeue list 0 -> 1 -> 2
                // just grab the head of the list

                if (new_dequeue_head != NULL)
                {
                    (void)interlocked_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, new_dequeue_head->next);

                    // set return to be the dequeue list head
                    /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_010: [ On success it shall return a pointer to the dequeued item. ]*/
                    /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_019: [ If the queue is empty, mpsc_lock_free_queue_dequeue shall return NULL. ]*/
                    result = new_dequeue_head;
                }
            }
        }
    }

    return result;
}

MPSC_LOCK_FREE_QUEUE_ITEM* mpsc_lock_free_queue_peek(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue)
{
    MPSC_LOCK_FREE_QUEUE_ITEM* result;

    if (mpsc_lock_free_queue == NULL)
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_027: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_peek shall fail and return NULL. ]*/
        LogError("NULL mpsc_lock_free_queue");
        result = NULL;
    }
    else
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_025: [ mpsc_lock_free_queue_peek shall return the oldest item in the queue without removing it. ]*/

        // it is assumed that destroy is not called concurrently with peek

        // there is only one consumer, so we're guaranteed by the semantics of the queue that noone else is peeking/removing
        // items. Items could be added at any rate but we do not care about that

        // first simply get the head of the dequeue list
        result = interlocked_compare_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, NULL, NULL);
        if (result == NULL)
        {
            // nothing in the dequeue list, so we have to move some items from the enqueue list
            // simply set the head to NULL and grab the entire list
            MPSC_LOCK_FREE_QUEUE_ITEM* temporary_list = interlocked_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->enqueue_head, NULL);

            if (temporary_list != NULL)
            {
                // now that the items are in the temporary list, we simply have to put them in reverse order in the dequeue list,
                // since in the dequeue list they will be pulled out LIFO.
                // enqueue order: 0, 1, 2
                // They ended up in the enqueue list being: 2 -> 1 -> 0, where 2 is the head.
                // We need to get them to be 0 -> 1 -> 2 in the dequeue list.

                MPSC_LOCK_FREE_QUEUE_ITEM* new_dequeue_head = NULL;
                MPSC_LOCK_FREE_QUEUE_ITEM* current_item = temporary_list;

                while (current_item != NULL)
                {
                    MPSC_LOCK_FREE_QUEUE_ITEM* next_item = current_item->next;
                    current_item->next = new_dequeue_head;
                    new_dequeue_head = current_item;
                    current_item = next_item;
                }

                // now we got in the dequeue list 0 -> 1 -> 2
                // just grab the head of the list

                (void)interlocked_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, new_dequeue_head);

                // set return to be the dequeue list head
                /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_026: [ On success it shall return a non-NULL pointer to the peeked item. ]*/
                /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_028: [ If the queue is empty, mpsc_lock_free_queue_peek shall return NULL. ]*/
                result = new_dequeue_head;
            }
        }
    }

    return result;
}

int mpsc_lock_free_queue_is_empty(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue, bool* is_empty)
{
    int result;

    if ((mpsc_lock_free_queue == NULL) ||
        (is_empty == NULL))
    {
        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_024: [ If mpsc_lock_free_queue or is_empty is NULL then mpsc_lock_free_queue_is_empty shall fail and return a non-zero value. ]*/
        LogError("Bad arguments: mpsc_lock_free_queue = %p, is_empty = %p",
            mpsc_lock_free_queue, is_empty);
        result = MU_FAILURE;
    }
    else
    {
        // it is assumed that destroy is not called concurrently with is_empty

        if (interlocked_compare_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->dequeue_head, NULL, NULL) != NULL)
        {
            /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_021: [ mpsc_lock_free_queue_is_empty shall set is_empty to false if at least one item is in the queue. ]*/
            *is_empty = false;
        }
        else
        {
            if (interlocked_compare_exchange_pointer((void* volatile_atomic*) &mpsc_lock_free_queue->enqueue_head, NULL, NULL) != NULL)
            {
                /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_021: [ mpsc_lock_free_queue_is_empty shall set is_empty to false if at least one item is in the queue. ]*/
                *is_empty = false;
            }
            else
            {
                /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_022: [ mpsc_lock_free_queue_is_empty shall set is_empty to true if no items are in the queue. ]*/
                *is_empty = true;
            }
        }

        /* Codes_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
        result = 0;
    }

    return result;
}
