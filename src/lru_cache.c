// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/sync.h"
#include "c_pal/interlocked.h"

#include "c_util/rc_string.h"
#include "c_pal/thandle.h"
#include "c_pal/srw_lock.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#include "clds/lru_cache.h"


typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE clds_hazard_pointers_thread_helper;

    CLDS_HASH_TABLE_HANDLE table;

    int64_t current_size;
    uint64_t capacity;

    volatile_atomic int64_t* seq_no;

    DLIST_ENTRY head;

    SRW_LOCK_HANDLE lock;
} LRU_CACHE;

typedef struct LRU_NODE_TAG
{
    void* key;
    int64_t size;
    CLDS_HASH_TABLE_ITEM* value;
    DLIST_ENTRY node;
} LRU_NODE;
DECLARE_HASH_TABLE_NODE_TYPE(LRU_NODE);

LRU_CACHE_HANDLE lru_cache_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context, uint64_t capacity)
{
    LRU_CACHE_HANDLE result;

    if (
        /*Codes_SRS_LRU_CACHE_13_001: [ If compute_hash is NULL, lru_cache_create shall fail and return NULL. ]*/
        (compute_hash == NULL) ||
        /*Codes_SRS_LRU_CACHE_13_002: [ If key_compare_func is NULL, lru_cache_create shall fail and return NULL. ]*/
        (key_compare_func == NULL) ||
        /*Codes_SRS_LRU_CACHE_13_003: [ If initial_bucket_size is 0, lru_cache_create shall fail and return NULL. ]*/
        (initial_bucket_size == 0) ||
        /*Codes_SRS_LRU_CACHE_13_004: [ If clds_hazard_pointers is NULL, lru_cache_create shall fail and return NULL. ]*/
        (clds_hazard_pointers == NULL) ||
        /*Codes_SRS_LRU_CACHE_13_010: [ If capacity less than or equals to 0, then lru_cache_create shall fail and return NULL. ]*/
        (capacity <= 0) ||
        /*Codes_SRS_LRU_CACHE_13_009: [ If start_sequence_number is NULL, then skipped_seq_no_cb must also be NULL, otherwise lru_cache_create shall fail and return NULL. ]*/
        ((start_sequence_number == NULL) && (skipped_seq_no_cb != NULL))
        )
    {
        LogError("Invalid arguments: COMPUTE_HASH_FUNC compute_hash=%p, KEY_COMPARE_FUNC key_compare_func=%p, size_t initial_bucket_size=%zu, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p, volatile_atomic int64_t* start_sequence_number=%p, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb=%p, void* skipped_seq_no_cb_context=%p",
            compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
        result = NULL;
    }
    else
    {
        /*Codes_SRS_LRU_CACHE_13_011: [ lru_cache_create shall allocate memory for LRU_CACHE_HANDLE. ]*/
        LRU_CACHE_HANDLE lru_cache = malloc(sizeof(LRU_CACHE));
        if (lru_cache == NULL)
        {
            /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
            LogError("malloc failed for LRU_CACHE_HANDLE");
            result = NULL;
        }
        else
        {
            /*Codes_SRS_LRU_CACHE_13_005: [ start_sequence_number shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. ]*/
            /*Codes_SRS_LRU_CACHE_13_012: [ lru_cache_create shall assign start_sequence_number to seq_no if not NULL, otherwise seq_no is defaulted to 0. ]*/
            if (start_sequence_number == NULL)
            {
                interlocked_exchange_64(lru_cache->seq_no, 0);
            }
            else
            {
                lru_cache->seq_no = start_sequence_number;
            }

            /*Codes_SRS_LRU_CACHE_13_013: [ lru_cache_create shall assign clds_hazard_pointers to LRU_CACHE_HANDLE. ]*/
            lru_cache->clds_hazard_pointers = clds_hazard_pointers;

            /*Codes_SRS_LRU_CACHE_13_014: [ lru_cache_create shall allocate CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE by calling clds_hazard_pointers_thread_helper_create. ]*/
            lru_cache->clds_hazard_pointers_thread_helper = clds_hazard_pointers_thread_helper_create(lru_cache->clds_hazard_pointers);
            if (lru_cache->clds_hazard_pointers_thread_helper == NULL)
            {
                /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                LogError("clds_hazard_pointers_thread_helper_create failed");
                result = NULL;
            }
            else
            {
                /*Codes_SRS_LRU_CACHE_13_015: [ lru_cache_create shall allocate clds_hash_table by calling clds_hash_table_create. ]*/
                lru_cache->table = clds_hash_table_create(compute_hash, key_compare_func, initial_bucket_size, lru_cache->clds_hazard_pointers, lru_cache->seq_no, skipped_seq_no_cb, skipped_seq_no_cb_context);
                if (lru_cache->table == NULL)
                {
                    /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                    LogError("Cannot create table, clds_hash_table_create failed");
                    result = NULL;
                }
                else
                {
                    /*Codes_SRS_LRU_CACHE_13_016: [ lru_cache_create shall allocate SRW_LOCK_HANDLE by calling srw_lock_create. ]*/
                    lru_cache->lock = srw_lock_create(false, "lru_cache_lock");
                    if (lru_cache->lock == NULL)
                    {
                        /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                        LogError("failure in srw_lock_create(false, \"lru_cache_lock\")");
                        result = NULL;
                    }
                    else
                    {
                        /*Codes_SRS_LRU_CACHE_13_017: [ lru_cache_create shall initialize head by calling DList_InitializeListHead. ]*/
                        DList_InitializeListHead(&(lru_cache->head));

                        /*Codes_SRS_LRU_CACHE_13_018: [ lru_cache_create shall assign default value of 0 to current_size and capacity. ]*/
                        lru_cache->current_size = 0;
                        lru_cache->capacity = capacity;

                        /*Codes_SRS_LRU_CACHE_13_019: [ On success, lru_cache_create shall return LRU_CACHE_HANDLE. ]*/
                        result = lru_cache;
                        goto all_ok;
                    }
                    clds_hash_table_destroy(lru_cache->table);
                }
                clds_hazard_pointers_thread_helper_destroy(lru_cache->clds_hazard_pointers_thread_helper);
            }
            free(lru_cache);
        }
    }
all_ok:
    return result;
}

void lru_cache_destroy(LRU_CACHE_HANDLE lru_cache)
{
    if (lru_cache == NULL)
    {
        /*Codes_SRS_LRU_CACHE_13_021: [ If lru_cache is NULL, then lru_cache_destroy shall fail. ]*/
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p", lru_cache);
    }
    else
    {
        /*Codes_SRS_LRU_CACHE_13_022: [ lru_cache_destroy shall free all resources associated with the LRU_CACHE_HANDLE. ]*/
        srw_lock_destroy(lru_cache->lock);
        clds_hash_table_destroy(lru_cache->table);
        //clds_hazard_pointers_destroy(lru_cache->clds_hazard_pointers);
        clds_hazard_pointers_thread_helper_destroy(lru_cache->clds_hazard_pointers_thread_helper);
        free(lru_cache);
    }
}

static LRU_CACHE_PUT_RESULT add_to_cache_internal(LRU_CACHE_HANDLE lru_cache, CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t size, int64_t* insert_seq_no)
{
    /*Codes_SRS_LRU_CACHE_13_044: [ lru_cache_put shall create LRU Node item to be inserted in the hash table. ]*/
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(LRU_NODE, NULL, NULL);
    LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, item);
    new_node->key = key;
    new_node->size = size;
    new_node->value = value;

    /*Codes_SRS_LRU_CACHE_13_045: [ lru_cache_put shall insert the LRU Node item in the hash table by calling clds_hash_table_insert. ]*/
    CLDS_HASH_TABLE_INSERT_RESULT hash_table_insert = clds_hash_table_insert(lru_cache->table, hazard_pointers_thread, key, item, insert_seq_no);

    if (hash_table_insert != CLDS_HASH_TABLE_INSERT_OK)
    {
        /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, item);
        return LRU_CACHE_PUT_ERROR;
    }
    else
    {
        /*Codes_SRS_LRU_CACHE_13_046: [ lru_cache_put shall acquire the lock in exclusive mode. ]*/
        srw_lock_acquire_exclusive(lru_cache->lock);
        {
            /*Codes_SRS_LRU_CACHE_13_047: [ lru_cache_put shall append the node to the tail and update the current_size with item size. ]*/
            DList_InitializeListHead(&(new_node->node));
            DList_InsertTailList(&(lru_cache->head), &(new_node->node));
            lru_cache->current_size += size;
        }
        /*Codes_SRS_LRU_CACHE_13_048: [ lru_cache_put shall release the lock in exclusive mode. ]*/
        srw_lock_release_exclusive(lru_cache->lock);
    }
    /*Codes_SRS_LRU_CACHE_13_049: [ On success, lru_cache_put shall return LRU_CACHE_PUT_OK. ]*/
    return LRU_CACHE_PUT_OK;
}

LRU_CACHE_PUT_RESULT lru_cache_put(LRU_CACHE_HANDLE lru_cache, void* key, CLDS_HASH_TABLE_ITEM* value, uint64_t size, int64_t* seq_no, LRU_CACHE_EVICT_CALLBACK_FUNC evict_callback, void* context)
{
    LRU_CACHE_PUT_RESULT result = LRU_CACHE_PUT_OK;

    if (/*Codes_SRS_LRU_CACHE_13_023: [ If lru_handle is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        lru_cache == NULL ||
        /*Codes_SRS_LRU_CACHE_13_024: [ If key is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        key == NULL ||
        /*Codes_SRS_LRU_CACHE_13_025: [ If value is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        value == NULL ||
        /*Codes_SRS_LRU_CACHE_13_026: [ If size is 0, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        size == 0)
    {
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p, void* key=%p, CLDS_HASH_TABLE_ITEM* value=%p, int64_t size=%" PRIu64 ", int64_t seq_no=%" PRIu64 "", lru_cache, key, value, size, seq_no);
        result = LRU_CACHE_PUT_ERROR;
    }
    else
    {
        bool can_continue = (result == LRU_CACHE_PUT_OK);

        if (lru_cache->capacity < size)
        {
            /*Codes_SRS_LRU_CACHE_13_027: [ If size is greater than capacity of lru cache, then lru_cache_put shall fail and return LRU_CACHE_PUT_VALUE_INVALID_SIZE. ]*/
            LogError("value size is larger than capacity.");
            result = LRU_CACHE_PUT_VALUE_INVALID_SIZE;
        }
        else
        {

            /*Codes_SRS_LRU_CACHE_13_028: [ lru_cache_put shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
            CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(lru_cache->clds_hazard_pointers_thread_helper);
            if (hazard_pointers_thread == NULL)
            {
                /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                LogError("clds_hazard_pointers_thread_helper_get_thread failed.");
                result = LRU_CACHE_PUT_ERROR;
            }

            /*Codes_SRS_LRU_CACHE_13_029: [ lru_cache_put shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
            CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, hazard_pointers_thread, key);
            if (hash_table_item != NULL)
            {
                /*Codes_SRS_LRU_CACHE_13_030: [ If the key is found: ]*/

                LRU_NODE* current_item = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
                PDLIST_ENTRY node = &(current_item->node);

                CLDS_HASH_TABLE_ITEM* old_item;

                /*Codes_SRS_LRU_CACHE_13_033: [ lru_cache_put shall acquire the lock in exclusive mode. ]*/
                srw_lock_acquire_exclusive(lru_cache->lock);

                /*Codes_SRS_LRU_CACHE_13_063: [ If the cache has enough capacity to accommodate the new value: ]*/
                if ((lru_cache->current_size - current_item->size + size) < lru_cache->capacity)
                {
                    /*Codes_SRS_LRU_CACHE_13_064: [ lru_cache_put shall create LRU Node item to be updated in the hash table. ]*/
                    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(LRU_NODE, NULL, NULL);
                    LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, item);
                    new_node->key = key;
                    new_node->size = size;
                    new_node->value = value;

                    /*Codes_SRS_LRU_CACHE_13_065: [ lru_cache_put shall update the LRU Node item in the hash table by calling clds_hash_table_set_value. ]*/
                    if (clds_hash_table_set_value(lru_cache->table, hazard_pointers_thread, key, item, NULL, NULL, &old_item, seq_no) != CLDS_HASH_TABLE_SET_VALUE_OK)
                    {
                        /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                        LogError("Cannot set old key=%p from the clds_hash_table", key);
                        can_continue = false;
                        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, item);
                        result = LRU_CACHE_PUT_ERROR;
                    }
                    else
                    {

                        DList_RemoveEntryList(node);
                        DList_InitializeListHead(&(new_node->node));
                        /*Codes_SRS_LRU_CACHE_13_066: [ lru_cache_put shall append the updated node to the tail to maintain the order. ]*/
                        DList_InsertTailList(&(lru_cache->head), &(new_node->node));

                        /*Codes_SRS_LRU_CACHE_13_070: [ lru_cache_put shall update the current_size with the new size. ]*/
                        lru_cache->current_size -= current_item->size;
                        lru_cache->current_size += size;

                        /*Codes_SRS_LRU_CACHE_13_067: [ lru_cache_put shall free the old value. ]*/
                        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, old_item);

                        can_continue = false;

                        /*Codes_SRS_LRU_CACHE_13_068: [ lru_cache_put shall return with LRU_CACHE_EVICT_OK. ]*/
                        result = LRU_CACHE_EVICT_OK;
                    }

                }
                /*Codes_SRS_LRU_CACHE_13_069: [ If the cache does not have enough capacity to accommodate the new value: ]*/
                else
                {
                    /*Codes_SRS_LRU_CACHE_13_031: [ lru_cache_put shall remove the old value from the clds_hash_table. ]*/
                    if (clds_hash_table_remove(lru_cache->table, hazard_pointers_thread, key, &old_item, seq_no) != CLDS_HASH_TABLE_REMOVE_OK)
                    {
                        /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                        LogError("Cannot remove old key=%p from the clds_hash_table", key);
                        can_continue = false;
                        result = LRU_CACHE_PUT_ERROR;
                    }
                    else
                    {
                        /*Codes_SRS_LRU_CACHE_13_032: [ lru_cache_put shall free the old value. ]*/
                        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, old_item);

                        /*Codes_SRS_LRU_CACHE_13_034: [ lru_cache_put shall subtract old value size from cache current_size. ]*/
                        lru_cache->current_size -= current_item->size;

                        /*Codes_SRS_LRU_CACHE_13_035: [ lru_cache_put shall remove the old value node from doubly_linked_list by calling DList_RemoveEntryList. ]*/
                        DList_RemoveEntryList(node);
                    }
                }

                /*Codes_SRS_LRU_CACHE_13_036: [ lru_cache_put shall release the lock in exclusive mode. ]*/
                srw_lock_release_exclusive(lru_cache->lock);

                CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, hash_table_item);
            }

            do
            {
                srw_lock_acquire_shared(lru_cache->lock);
                {
                    if (!can_continue || DList_IsListEmpty(&lru_cache->head) || (lru_cache->current_size + size < lru_cache->capacity))
                    {
                        srw_lock_release_shared(lru_cache->lock);
                        break;
                    }
                    srw_lock_release_shared(lru_cache->lock);
                }

                /*Codes_SRS_LRU_CACHE_13_037: [ While the capacity of the cache is full: ]*/
                /*Codes_SRS_LRU_CACHE_13_038: [ lru_cache_put shall get the least used node. ]*/
                DLIST_ENTRY* least_used_node = lru_cache->head.Flink;
                LRU_NODE* least_used_node_value = (LRU_NODE*)CONTAINING_RECORD(least_used_node, LRU_NODE, node);

                CLDS_HASH_TABLE_ITEM* entry;
                /*Codes_SRS_LRU_CACHE_13_039: [ The least used node is removed from clds_hash_table by calling clds_hash_table_remove. ]*/
                if (clds_hash_table_remove(lru_cache->table, hazard_pointers_thread, least_used_node_value->key, &entry, seq_no) != CLDS_HASH_TABLE_REMOVE_OK)
                {
                    /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                    LogError("Error removing item from hash table");
                    can_continue = false;
                    result = MU_FAILURE;
                    evict_callback(context, LRU_CACHE_EVICT_ERROR, NULL);
                }
                else
                {
                    /*Codes_SRS_LRU_CACHE_13_040: [ lru_cache_put shall acquire the lock in exclusive. ]*/
                    srw_lock_acquire_exclusive(lru_cache->lock);
                    {
                        /*Codes_SRS_LRU_CACHE_13_041: [ lru_cache_put shall decrement the least used node size from current_size and remove it from the DList by calling DList_RemoveEntryList. ]*/
                        lru_cache->current_size -= least_used_node_value->size;
                        (void)DList_RemoveEntryList(least_used_node);
                    }
                    /*Codes_SRS_LRU_CACHE_13_042: [ lru_cache_put shall release the lock in exclusive mode. ]*/
                    srw_lock_release_exclusive(lru_cache->lock);

                    /*Codes_SRS_LRU_CACHE_13_043: [ On success, evict_callback is called with the status LRU_CACHE_EVICT_OK. ]*/
                    evict_callback(context, LRU_CACHE_EVICT_OK, least_used_node_value->value);

                    CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, entry);
                }
            } while (1);

            if (can_continue)
            {
                result = add_to_cache_internal(lru_cache, hazard_pointers_thread, key, value, size, seq_no);
            }
        }
    }
    return result;
}


CLDS_HASH_TABLE_ITEM* lru_cache_get(LRU_CACHE_HANDLE lru_cache, void* key)
{
    CLDS_HASH_TABLE_ITEM* result = NULL;

    if (
        /*Codes_SRS_LRU_CACHE_13_051: [ If lru_cache is NULL, then lru_cache_get shall fail and return NULL. ]*/
        lru_cache == NULL ||
        /*Codes_SRS_LRU_CACHE_13_052: [ If key is NULL, then lru_cache_get shall fail and return NULL. ]*/
        key == NULL)
    {
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p, void* key=%p", lru_cache, key);
        result = NULL;
    }
    else
    {
        /*Codes_SRS_LRU_CACHE_13_053: [ lru_cache_get shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
        CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(lru_cache->clds_hazard_pointers_thread_helper);
        if (hazard_pointers_thread == NULL)
        {
            /*Codes_SRS_LRU_CACHE_13_061: [ If there are any failures, lru_cache_get shall return NULL. ]*/
            LogError("clds_hazard_pointers_thread_helper_get_thread failed");
            result = NULL;
        }
        else
        {
            /*Codes_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
            CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, hazard_pointers_thread, key);
            if (hash_table_item != NULL)
            {

                LRU_NODE* doubly_value = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
                PDLIST_ENTRY node = &(doubly_value->node);

                /*Codes_SRS_LRU_CACHE_13_056: [ lru_cache_get shall acquire the lock in exclusive mode. ]*/
                srw_lock_acquire_exclusive(lru_cache->lock);
                {
                    /*Codes_SRS_LRU_CACHE_13_055: [ If the key is found and the node from the key is not recently used: ]*/
                    if (lru_cache->head.Blink != node)
                    {
                        /*Codes_SRS_LRU_CACHE_13_057: [ lru_cache_get shall remove the old value node from doubly_linked_list by calling DList_RemoveEntryList. ]*/
                        DList_RemoveEntryList(node);
                        DList_InitializeListHead(node);
                        /*Codes_SRS_LRU_CACHE_13_058: [ lru_cache_get shall make the node as the tail by calling DList_InsertTailList. ]*/
                        DList_InsertTailList(&lru_cache->head, node);
                    }
                }
                /*Codes_SRS_LRU_CACHE_13_059: [ lru_cache_get shall release the lock in exclusive mode. ]*/
                srw_lock_release_exclusive(lru_cache->lock);
                result = doubly_value->value;
                CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, hash_table_item);
            }
        }
    }

    /*Codes_SRS_LRU_CACHE_13_060: [ On success, lru_cache_get shall return CLDS_HASH_TABLE_ITEM value of the key. ]*/
    return result;
}