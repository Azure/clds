// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "windows.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_atomics.h"
#include "clds/clds_st_hash_set.h"

#define DEFAULT_RECLAIM_THRESHOLD 1

typedef struct CLDS_HAZARD_POINTER_RECORD_TAG
{
    void* node;
    struct CLDS_HAZARD_POINTER_RECORD_TAG* next;
} CLDS_HAZARD_POINTER_RECORD;

typedef struct CLDS_RECLAIM_LIST_ENTRY_TAG
{
    struct CLDS_RECLAIM_LIST_ENTRY_TAG* next;
    void* node;
    RECLAIM_FUNC reclaim;
} CLDS_RECLAIM_LIST_ENTRY;

typedef struct CLDS_HAZARD_POINTERS_THREAD_TAG
{
    volatile struct CLDS_HAZARD_POINTERS_THREAD_TAG* next;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTER_RECORD* free_pointers;
    CLDS_HAZARD_POINTER_RECORD* pointers;
    CLDS_RECLAIM_LIST_ENTRY* reclaim_list;
    volatile LONG active;
    size_t reclaim_list_entry_count;
} CLDS_HAZARD_POINTERS_THREAD;

typedef struct CLDS_HAZARD_POINTERS_TAG
{
    size_t reclaim_threshold;
    volatile CLDS_HAZARD_POINTERS_THREAD* head;
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

static void internal_reclaim(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_thread->clds_hazard_pointers;
    CLDS_ST_HASH_SET_HANDLE all_hps_set = clds_st_hash_set_create(hp_key_hash, hp_key_compare, clds_hazard_pointers_thread->clds_hazard_pointers->reclaim_threshold);
    if (all_hps_set == NULL)
    {
        // oops, panic now!
        LogError("Cannot allocate hazard pointers hash set");
    }
    else
    {
        // go through all hazard pointers of all threads, no thread should be able to get a hazard pointer after this point
        CLDS_HAZARD_POINTERS_THREAD_HANDLE current_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointerAcquire((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
        while (current_thread != NULL)
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE next_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointerAcquire((volatile PVOID*)&current_thread->next, NULL, NULL);
            if (InterlockedAddNoFence(&current_thread->active, 0) == 1)
            {
                // look at the pointers of this thread
                // if it gets unregistered in the meanwhile we won't care
                // if it gets registered again we also don't care as for sure it does not have our hazard pointer anymore
                CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointerAcquire((volatile PVOID*)&current_thread->pointers, NULL, NULL);
                while (clds_hazard_pointer != NULL)
                {
                    CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_pointer = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointerAcquire((volatile PVOID*)&clds_hazard_pointer->next, NULL, NULL);
                    void* node = InterlockedCompareExchangePointerAcquire(&clds_hazard_pointer->node, NULL, NULL);
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
}

CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers_create(void)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;

    clds_hazard_pointers = (CLDS_HAZARD_POINTERS_HANDLE)malloc(sizeof(CLDS_HAZARD_POINTERS));
    if (clds_hazard_pointers == NULL)
    {
        LogError("malloc failed");
    }
    else
    {
        clds_hazard_pointers->reclaim_threshold = DEFAULT_RECLAIM_THRESHOLD;
        (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL);
    }

    return clds_hazard_pointers;
}

void clds_hazard_pointers_destroy(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    if (clds_hazard_pointers == NULL)
    {
        LogError("Invalid arguments: CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p", clds_hazard_pointers);
    }
    else
    {
        CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
        while (clds_hazard_pointers_thread != NULL)
        {
            internal_reclaim(clds_hazard_pointers_thread);
            clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->next, NULL, NULL);
        }

        // free all thread data here
        clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
        while (clds_hazard_pointers_thread != NULL)
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE next_clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->next, NULL, NULL);
            CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL, NULL);
            while (hazard_ptr != NULL)
            {
                CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&hazard_ptr->next, NULL, NULL);
                free(hazard_ptr);
                hazard_ptr = next_hazard_ptr;
            }

            hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
            while (hazard_ptr != NULL)
            {
                CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&hazard_ptr->next, NULL, NULL);
                free(hazard_ptr);
                hazard_ptr = next_hazard_ptr;
            }

            free(clds_hazard_pointers_thread);
            clds_hazard_pointers_thread = next_clds_hazard_pointers_thread;
        }

        free(clds_hazard_pointers);
    }
}

CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_register_thread(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)malloc(sizeof(CLDS_HAZARD_POINTERS_THREAD));
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
            CLDS_HAZARD_POINTERS_THREAD_HANDLE current_threads_head = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
            clds_hazard_pointers_thread->next = current_threads_head;
            clds_hazard_pointers_thread->reclaim_list_entry_count = 0;
            (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL);
            (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL);
            (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->reclaim_list, NULL);
            (void)InterlockedExchange(&clds_hazard_pointers_thread->active, 1);
            if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, clds_hazard_pointers_thread, current_threads_head) != current_threads_head)
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
        // remove the thread from the thread list
        clds_hazard_pointers_thread->reclaim_list_entry_count = 0;
        clds_hazard_pointers_thread->reclaim_list = NULL;
        (void)InterlockedExchange(&clds_hazard_pointers_thread->active, 0);
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
            hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
            if (hazard_ptr != NULL)
            {
                if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, hazard_ptr->next, hazard_ptr) != hazard_ptr)
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
            hazard_ptr = (CLDS_HAZARD_POINTER_RECORD_HANDLE)malloc(sizeof(CLDS_HAZARD_POINTER_RECORD));
            if (hazard_ptr == NULL)
            {
                LogError("Error allocating hazard pointer");
                result = NULL;
            }
            else
            {
                CLDS_HAZARD_POINTER_RECORD* current_list_head;

                // add it to the hazard pointer list
                current_list_head = (CLDS_HAZARD_POINTER_RECORD*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

                hazard_ptr->node = node;
                hazard_ptr->next = current_list_head;
                
                (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, hazard_ptr);

                // inserted in the used hazard pointer list, we are done
                result = hazard_ptr;
            }
        }
        else
        {
            CLDS_HAZARD_POINTER_RECORD* current_list_head;

            // add it to the hazard pointer list
            current_list_head = (CLDS_HAZARD_POINTER_RECORD*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

            hazard_ptr->node = node;
            hazard_ptr->next = current_list_head;

            (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, hazard_ptr);

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
        CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer = (CLDS_HAZARD_POINTER_RECORD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL, NULL);

        (void)InterlockedExchangePointer(&clds_hazard_pointer_record->node, NULL);

        while (clds_hazard_pointer != NULL)
        {
            if (clds_hazard_pointer == clds_hazard_pointer_record)
            {
                if (previous_hazard_pointer != NULL)
                {
                    (void)InterlockedExchangePointer((volatile PVOID*)&previous_hazard_pointer->next, clds_hazard_pointer->next);
                }
                else
                {
                    (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, clds_hazard_pointer->next);
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
        clds_hazard_pointer_record->next = (CLDS_HAZARD_POINTER_RECORD*)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
        (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, clds_hazard_pointer_record);
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
        CLDS_RECLAIM_LIST_ENTRY* reclaim_list_entry = (CLDS_RECLAIM_LIST_ENTRY*)malloc(sizeof(CLDS_RECLAIM_LIST_ENTRY));
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

            // add the pointer to the reclaim list, no other thread has access to this list, so no Interlocked needed
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
