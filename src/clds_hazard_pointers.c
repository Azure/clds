// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// This whole module has to be rewritten with unit tests
// That will force breaking it down in the proper units.
// Unfortunately at the moment of writing this comment it would be too much of a task for the fix we need to make
// to alleviate the perf issue of accumulating HP thread handles over time
// Task:
// ID Work Item Type Title State Assigned To Iteration Path Effort Remaining Work Original Estimate Value Area Tags
// 25697532 Task Rewrite with tests clds HP To Do   One\Custom\AzureMessaging\Germanium\GeM1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/interlocked.h"
#include "c_pal/log_critical_and_terminate.h"

#include "c_util/worker_thread.h"

#include "clds/clds_st_hash_set.h"
#include "clds/inactive_hp_thread_queue.h"

#include "clds/clds_hazard_pointers.h"

#define DEFAULT_RECLAIM_THRESHOLD 1

#define INITIAL_INACTIVE_THREADS_QUEUE_SIZE 16
// Let's have a decent number, if we have 16K inactive threads we'd have issues iterating over them anyway
#define MAX_INACTIVE_THREADS_QUEUE_SIZE 16384

typedef struct CLDS_HAZARD_POINTER_RECORD_TAG
{
    void* volatile_atomic node;
    struct CLDS_HAZARD_POINTER_RECORD_TAG* volatile_atomic next;
} CLDS_HAZARD_POINTER_RECORD;

typedef struct CLDS_RECLAIM_LIST_ENTRY_TAG
{
    struct CLDS_RECLAIM_LIST_ENTRY_TAG* next;
    void* node;
    RECLAIM_FUNC reclaim;
} CLDS_RECLAIM_LIST_ENTRY;

typedef struct CLDS_HAZARD_POINTERS_THREAD_TAG
{
    struct CLDS_HAZARD_POINTERS_THREAD_TAG* volatile_atomic next;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTER_RECORD* free_pointers;
    CLDS_HAZARD_POINTER_RECORD* volatile_atomic pointers;
    CLDS_RECLAIM_LIST_ENTRY* reclaim_list;
    volatile_atomic int32_t active;
    size_t reclaim_list_entry_count;
} CLDS_HAZARD_POINTERS_THREAD;

typedef struct CLDS_HAZARD_POINTERS_TAG
{
    WORKER_THREAD_HANDLE hp_thread_cleanup_worker;
    size_t reclaim_threshold;
    CLDS_HAZARD_POINTERS_THREAD* volatile_atomic head;
    // This epoch exists in order to make sure that no HP thread information that is still being accessed
    // is actually freed
    // HP Thread data is only freed when it was placed in the inactive queue and its epoch is lower than the current epoch
    // The epoch is being incremented whenever there are no more reclaims being executed (this means noone would be involving the inactive threads in the scan)
    volatile_atomic int64_t epoch; // epoch for inactive threads
    // This is a counter maintaining how many pending reclaim calls we have
    volatile_atomic int32_t pending_reclaim_calls;
    // This is the queue of inactive threads
    TQUEUE(CLDS_HP_INACTIVE_THREAD) inactive_threads;
} CLDS_HAZARD_POINTERS;

static uint64_t hp_key_hash(void* key)
{
    return (uint64_t)key;
}

static int hp_key_compare(void* key1, void* key2)
{
    int result;

    if (key1 < key2)
    {
        result = -1;
    }
    else if (key1 > key2)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

// adds a hp thread handle to the inactive queue, stamping on it the current epoch number.
// The epoch number is incremented any time there are no pending reclaim calls
// we know that once a thread is added to the inactive queue it cannot be scanned by any thread any longer, so it is safe to free it
static int add_to_inactive_threads(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, CLDS_HAZARD_POINTERS_THREAD_HANDLE inactive_hp_thread_handle)
{
    int result;

    CLDS_HP_INACTIVE_THREAD inactive_thread = { .current_epoch_value = interlocked_add_64(&clds_hazard_pointers->epoch, 0), .hp_thread_handle = inactive_hp_thread_handle };
    if (TQUEUE_PUSH(CLDS_HP_INACTIVE_THREAD)(clds_hazard_pointers->inactive_threads, &inactive_thread, NULL) != TQUEUE_PUSH_OK)
    {
        LogError("TQUEUE_PUSH(CLDS_HP_INACTIVE_THREAD)(clds_hazard_pointers->inactive_threads=%p, inactive_hp_thread_handle=%p) failed", clds_hazard_pointers->inactive_threads, inactive_hp_thread_handle);
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }

    return result;
}

static void free_thread_data(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_ptr = interlocked_compare_exchange_pointer((void* volatile_atomic*) & clds_hazard_pointers_thread->pointers, NULL, NULL);
    while (hazard_ptr != NULL)
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_ptr = interlocked_compare_exchange_pointer((void* volatile_atomic*) & hazard_ptr->next, NULL, NULL);
        free(hazard_ptr);
        hazard_ptr = next_hazard_ptr;
    }

    hazard_ptr = interlocked_compare_exchange_pointer((void* volatile_atomic*) & clds_hazard_pointers_thread->free_pointers, NULL, NULL);
    while (hazard_ptr != NULL)
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_ptr = interlocked_compare_exchange_pointer((void* volatile_atomic*) & hazard_ptr->next, NULL, NULL);
        free(hazard_ptr);
        hazard_ptr = next_hazard_ptr;
    }

    free(clds_hazard_pointers_thread);
}

static bool is_epoch_lower(void* context, CLDS_HP_INACTIVE_THREAD* inactive_thread)
{
    int64_t* epoch = context;
    return inactive_thread->current_epoch_value < *epoch;
}

static void free_inactive_threads_in_previous_epochs(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, int64_t epoch)
{
    do
    {
        CLDS_HP_INACTIVE_THREAD inactive_thread;
        if (TQUEUE_POP(CLDS_HP_INACTIVE_THREAD)(clds_hazard_pointers->inactive_threads, &inactive_thread, NULL, is_epoch_lower, &epoch) != TQUEUE_POP_OK)
        {
            break;
        }

        // got one, free it
        free_thread_data(inactive_thread.hp_thread_handle);
    } while (1);
}

static void internal_reclaim(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_thread->clds_hazard_pointers;

    (void)interlocked_increment(&clds_hazard_pointers->pending_reclaim_calls);

    CLDS_ST_HASH_SET_HANDLE all_hps_set = clds_st_hash_set_create(hp_key_hash, hp_key_compare, clds_hazard_pointers_thread->clds_hazard_pointers->reclaim_threshold);
    if (all_hps_set == NULL)
    {
        // oops, panic now!
        LogError("Cannot allocate hazard pointers hash set");
    }
    else
    {
        // go through all hazard pointers of all threads, no thread should be able to get a hazard pointer after this point
        CLDS_HAZARD_POINTERS_THREAD_HANDLE current_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, NULL, NULL);
        while (current_thread != NULL)
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE next_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_thread->next, NULL, NULL);
            if (interlocked_add(&current_thread->active, 0) == 1)
            {
                // look at the pointers of this thread
                // if it gets unregistered in the meanwhile we won't care
                // if it gets registered again we also don't care as for sure it does not have our hazard pointer anymore
                CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_thread->pointers, NULL, NULL);
                while (clds_hazard_pointer != NULL)
                {
                    CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_pointer = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointer->next, NULL, NULL);
                    void* node = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointer->node, NULL, NULL);
                    if (node != NULL)
                    {
                        CLDS_ST_HASH_SET_INSERT_RESULT insert_result = clds_st_hash_set_insert(all_hps_set, node);
                        if ((insert_result == CLDS_ST_HASH_SET_INSERT_OK) ||
                            (insert_result == CLDS_ST_HASH_SET_INSERT_KEY_ALREADY_EXISTS))
                        {
                            // all ok
                        }
                        else
                        {
                            LogError("Cannot insert hazard pointer in set");
                            break;
                        }
                    }

                    clds_hazard_pointer = next_hazard_pointer;
                }

                if (clds_hazard_pointer != NULL)
                {
                    break;
                }
            }

            current_thread = next_thread;
        }

        if (current_thread != NULL)
        {
            LogError("Error collecting hazard pointers");
        }
        else
        {
            // go through all pointers in the reclaim list
            CLDS_RECLAIM_LIST_ENTRY* current_reclaim_entry = clds_hazard_pointers_thread->reclaim_list;
            CLDS_RECLAIM_LIST_ENTRY* prev_reclaim_entry = NULL;
            while (current_reclaim_entry != NULL)
            {
                // this is the scan for the pointers
                CLDS_ST_HASH_SET_FIND_RESULT find_result = clds_st_hash_set_find(all_hps_set, current_reclaim_entry->node);

                if (find_result == CLDS_ST_HASH_SET_FIND_ERROR)
                {
                    LogError("Error finding pointer");
                    break;
                }
                else if (find_result == CLDS_ST_HASH_SET_FIND_NOT_FOUND)
                {
                    // node is safe to be reclaimed
                    current_reclaim_entry->reclaim(current_reclaim_entry->node);

                    // now remove it from the reclaim list
                    if (prev_reclaim_entry == NULL)
                    {
                        // this is the head of the reclaim list
                        clds_hazard_pointers_thread->reclaim_list = current_reclaim_entry->next;
                        free(current_reclaim_entry);
                        current_reclaim_entry = clds_hazard_pointers_thread->reclaim_list;
                    }
                    else
                    {
                        prev_reclaim_entry->next = current_reclaim_entry->next;
                        free(current_reclaim_entry);
                        current_reclaim_entry = prev_reclaim_entry->next;
                    }

                    clds_hazard_pointers_thread->reclaim_list_entry_count--;
                }
                else
                {
                    // not safe, sorry, shall still have it around, move to next reclaim entry
                    prev_reclaim_entry = current_reclaim_entry;
                    current_reclaim_entry = current_reclaim_entry->next;
                }
            }
        }

        clds_st_hash_set_destroy(all_hps_set);
    }

    int64_t current_epoch = interlocked_add_64(&clds_hazard_pointers->epoch, 0);
    if (interlocked_decrement(&clds_hazard_pointers->pending_reclaim_calls) == 0)
    {
        if (interlocked_compare_exchange_64(&clds_hazard_pointers->epoch, current_epoch + 1, current_epoch) != current_epoch)
        {
            // epoch already incremented by other thread, let them do the cleanup
        }
        else
        {
            free_inactive_threads_in_previous_epochs(clds_hazard_pointers, current_epoch + 1);
        }
    }
}

// This function has the sole purpose of cleaning up inactive thread handles
// During normal operation, this is the only thread that removes CLDS_HAZARD_POINTERS_THREAD_HANDLEs from the list of CLDS_HAZARD_POINTERS_THREAD_HANDLE handles.
// The reclaim functions only look at the list, but they do not remove
// This is done so that only one background worker is looking at what needs to be cleaned up and there are not multiple threads attempting to remove from the list
static void hp_thread_cleanup_func(void* context)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = context;

    (void)clds_hazard_pointers;

    void* volatile_atomic* current_thread_address = (void* volatile_atomic*)&clds_hazard_pointers->head;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE current_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, NULL, NULL);
    while (current_thread != NULL)
    {
        CLDS_HAZARD_POINTERS_THREAD_HANDLE next_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&current_thread->next, NULL, NULL);
        if (interlocked_add(&current_thread->active, 0) == 1)
        {
            current_thread_address = (void* volatile_atomic*)&current_thread->next;
            current_thread = next_thread;
        }
        else
        {
            // remove from the list, no longer active
            if (interlocked_compare_exchange_pointer(current_thread_address, next_thread, current_thread) == current_thread)
            {
                // removed, great, we won't see this any longer

                // add it to the inactive threads
                if (add_to_inactive_threads(clds_hazard_pointers, current_thread) != 0)
                {
                    LogCriticalAndTerminate("Too many inactive threads");
                    // should not happen
                }

                // current_thread_address stays the same
                current_thread = next_thread;
            }
            else
            {
                // not removed, changed in the meanwhile, this should not really happen, but we don't care either
                break;
            }
        }
    }
}

CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers_create(void)
{
    CLDS_HAZARD_POINTERS_HANDLE result;

    result = malloc(sizeof(CLDS_HAZARD_POINTERS));
    if (result == NULL)
    {
        LogError("malloc failed");
    }
    else
    {
        result->hp_thread_cleanup_worker = worker_thread_create(hp_thread_cleanup_func, result);
        if (result->hp_thread_cleanup_worker == NULL)
        {
            LogError("worker_thread_create failed");
        }
        else
        {
            if (worker_thread_open(result->hp_thread_cleanup_worker) != 0)
            {
                LogError("worker_thread_open failed");
            }
            else
            {
                TQUEUE(CLDS_HP_INACTIVE_THREAD) inactive_threads = TQUEUE_CREATE(CLDS_HP_INACTIVE_THREAD)(INITIAL_INACTIVE_THREADS_QUEUE_SIZE, MAX_INACTIVE_THREADS_QUEUE_SIZE, NULL, NULL, NULL);
                if (inactive_threads == NULL)
                {
                    LogError("TQUEUE_CREATE(CLDS_HP_INACTIVE_THREAD)(INACTIVE_THREADS_QUEUE_SIZE, false) failed");
                }
                else
                {
                    TQUEUE_INITIALIZE_MOVE(CLDS_HP_INACTIVE_THREAD)(&result->inactive_threads, &inactive_threads);
                    result->reclaim_threshold = DEFAULT_RECLAIM_THRESHOLD;
                    (void)interlocked_exchange_pointer((void* volatile_atomic*) & result->head, NULL);
                    (void)interlocked_exchange_64(&result->epoch, 0);
                    (void)interlocked_exchange(&result->pending_reclaim_calls, 0);

                    goto all_ok;
                }

                worker_thread_close(result->hp_thread_cleanup_worker);
            }

            worker_thread_destroy(result->hp_thread_cleanup_worker);
        }

        free(result);
        result = NULL;
    }

all_ok:
    return result;
}

void clds_hazard_pointers_destroy(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    if (clds_hazard_pointers == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p", clds_hazard_pointers);
    }
    else
    {
        worker_thread_close(clds_hazard_pointers->hp_thread_cleanup_worker);

        worker_thread_destroy(clds_hazard_pointers->hp_thread_cleanup_worker);

        CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, NULL, NULL);
        while (clds_hazard_pointers_thread != NULL)
        {
            internal_reclaim(clds_hazard_pointers_thread);
            clds_hazard_pointers_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->next, NULL, NULL);
        }

        // free all thread data here
        clds_hazard_pointers_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, NULL, NULL);
        while (clds_hazard_pointers_thread != NULL)
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE next_clds_hazard_pointers_thread = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->next, NULL, NULL);

            free_thread_data(clds_hazard_pointers_thread);

            clds_hazard_pointers_thread = next_clds_hazard_pointers_thread;
        }

        // free all inactive threads
        free_inactive_threads_in_previous_epochs(clds_hazard_pointers, INT64_MAX);
        TQUEUE_ASSIGN(CLDS_HP_INACTIVE_THREAD)(&clds_hazard_pointers->inactive_threads, NULL);
        free(clds_hazard_pointers);
    }
}

CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_register_thread(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = malloc(sizeof(CLDS_HAZARD_POINTERS_THREAD));
    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("malloc failed");
    }
    else
    {
        bool restart_needed;

        clds_hazard_pointers_thread->clds_hazard_pointers = clds_hazard_pointers;
        do
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE current_threads_head = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, NULL, NULL);
            (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->next, current_threads_head);
            clds_hazard_pointers_thread->reclaim_list_entry_count = 0;
            (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, NULL);
            (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->free_pointers, NULL);
            clds_hazard_pointers_thread->reclaim_list = NULL;
            (void)interlocked_exchange(&clds_hazard_pointers_thread->active, 1);
            if (interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers->head, clds_hazard_pointers_thread, current_threads_head) != current_threads_head)
            {
                restart_needed = true;
            }
            else
            {
                // done
                restart_needed = false;
            }
        } while (restart_needed);
    }

    return clds_hazard_pointers_thread;
}

void clds_hazard_pointers_unregister_thread(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p", clds_hazard_pointers_thread);
    }
    else
    {
        CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_thread->clds_hazard_pointers;

        // remove the thread from the thread list
        (void)interlocked_exchange(&clds_hazard_pointers_thread->active, 0);

        WORKER_THREAD_SCHEDULE_PROCESS_RESULT schedule_result = worker_thread_schedule_process(clds_hazard_pointers->hp_thread_cleanup_worker);
        if (schedule_result != WORKER_THREAD_SCHEDULE_PROCESS_OK)
        {
            LogError("worker_thread_schedule_process failed with %" PRI_MU_ENUM "",
                MU_ENUM_VALUE(WORKER_THREAD_SCHEDULE_PROCESS_RESULT, schedule_result));
        }
    }
}

CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointers_acquire(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node)
{
    CLDS_HAZARD_POINTER_RECORD_HANDLE result;

    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* node=%p",
            clds_hazard_pointers_thread, node);
        result = NULL;
    }
    else
    {
        bool restart_needed;
        CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_ptr;

        // get a hazard pointer for the node from the free list
        do
        {
            hazard_ptr = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
            if (hazard_ptr != NULL)
            {
                if (interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->free_pointers, hazard_ptr->next, hazard_ptr) != hazard_ptr)
                {
                    restart_needed = true;
                }
                else
                {
                    // got it
                    restart_needed = false;
                }
            }
            else
            {
                // no free one
                restart_needed = false;
            }
        }
        while (restart_needed);

        if (hazard_ptr == NULL)
        {
            // no more pointers in free list, create one
            hazard_ptr = malloc(sizeof(CLDS_HAZARD_POINTER_RECORD));
            if (hazard_ptr == NULL)
            {
                LogError("Error allocating hazard pointer");
                result = NULL;
            }
            else
            {
                CLDS_HAZARD_POINTER_RECORD* current_list_head;

                // add it to the hazard pointer list
                current_list_head = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

                (void)interlocked_exchange_pointer(&hazard_ptr->node, node);
                hazard_ptr->next = current_list_head;
                
                (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, hazard_ptr);

                // inserted in the used hazard pointer list, we are done
                result = hazard_ptr;
            }
        }
        else
        {
            CLDS_HAZARD_POINTER_RECORD* current_list_head;

            // add it to the hazard pointer list
            current_list_head = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

            (void)interlocked_exchange_pointer(&hazard_ptr->node, node);
            hazard_ptr->next = current_list_head;

            (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, hazard_ptr);

            // inserted in the used hazard pointer list, we are done
            result = hazard_ptr;
        }
    }

    return result;
}

void clds_hazard_pointers_release(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer_record)
{
    if (clds_hazard_pointer_record == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer_record=%p",
            clds_hazard_pointers_thread, clds_hazard_pointer_record);
    }
    else
    {
        // remove it from the hazard pointers list for this thread, this thread is the only one removing
        // so no contention on the list
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hazard_pointer = NULL;
        CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

        (void)interlocked_exchange_pointer(&clds_hazard_pointer_record->node, NULL);

        while (clds_hazard_pointer != NULL)
        {
            if (clds_hazard_pointer == clds_hazard_pointer_record)
            {
                if (previous_hazard_pointer != NULL)
                {
                    (void)interlocked_exchange_pointer((void* volatile_atomic*)&previous_hazard_pointer->next, clds_hazard_pointer->next);
                }
                else
                {
                    (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->pointers, clds_hazard_pointer->next);
                }

                break;
            }
            else
            {
                previous_hazard_pointer = clds_hazard_pointer;
                clds_hazard_pointer = clds_hazard_pointer->next;
            }
        }

        // insert it in the free list
        struct CLDS_HAZARD_POINTER_RECORD_TAG* current_free_pointers = interlocked_compare_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
        (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointer_record->next, current_free_pointers);
        (void)interlocked_exchange_pointer((void* volatile_atomic*)&clds_hazard_pointers_thread->free_pointers, clds_hazard_pointer_record);
    }
}

void clds_hazard_pointers_reclaim(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node, RECLAIM_FUNC reclaim_func)
{
    if (
        (clds_hazard_pointers_thread == NULL) ||
        (node == NULL)
        )
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread=%p, void* node=%p, RECLAIM_FUNC reclaim_func=%p",
            clds_hazard_pointers_thread, node, reclaim_func);
    }
    else
    {
        CLDS_RECLAIM_LIST_ENTRY* reclaim_list_entry = malloc(sizeof(CLDS_RECLAIM_LIST_ENTRY));
        if (reclaim_list_entry == NULL)
        {
            // oops, panic now!
            LogError("Cannot allocate reclaim list entry");
        }
        else
        {
            reclaim_list_entry->next = clds_hazard_pointers_thread->reclaim_list;
            reclaim_list_entry->node = node;
            reclaim_list_entry->reclaim = reclaim_func;

            // add the pointer to the reclaim list, no other thread has access to this list, so no interlocked needed
            clds_hazard_pointers_thread->reclaim_list = reclaim_list_entry;
            clds_hazard_pointers_thread->reclaim_list_entry_count++;
            if (clds_hazard_pointers_thread->reclaim_list_entry_count >= clds_hazard_pointers_thread->clds_hazard_pointers->reclaim_threshold)
            {
                internal_reclaim(clds_hazard_pointers_thread);
            }
        }
    }
}

int clds_hazard_pointers_set_reclaim_threshold(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, size_t reclaim_threshold)
{
    int result;

    if (
        (clds_hazard_pointers == NULL) ||
        (reclaim_threshold == 0)
        )
    {
        LogError("Invalid arguments: clds_hazard_pointers = %p, reclaim_threshold = %zu",
            clds_hazard_pointers, reclaim_threshold);
        result = MU_FAILURE;
    }
    else
    {
        clds_hazard_pointers->reclaim_threshold = reclaim_threshold;
        result = 0;
    }

    return result;
}
