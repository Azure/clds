// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>

#include "c_logging/logger.h"

#include "c_pal/containing_record.h"
#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/sync.h"
#include "c_pal/interlocked.h"
#include "c_pal/srw_lock_ll.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#include "clds/lru_cache.h"

MU_DEFINE_ENUM_STRINGS(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);


typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE clds_hazard_pointers_thread_helper;

    CLDS_HASH_TABLE_HANDLE table;

    volatile_atomic int64_t current_size;
    int64_t capacity;

    DLIST_ENTRY head;

    SRW_LOCK_LL srw_lock;

    LRU_CACHE_ON_ERROR_CALLBACK_FUNC on_error_callback;
    void* on_error_context;
} LRU_CACHE;

typedef struct LRU_NODE_TAG
{
    void* key;
    int64_t size;
    void* value;
    DLIST_ENTRY node;

    LRU_CACHE_EVICT_CALLBACK_FUNC evict_callback;
    void* evict_callback_context;

    LRU_CACHE_VALUE_COPY copy_func;
    LRU_CACHE_VALUE_FREE free_func;
} LRU_NODE;
DECLARE_HASH_TABLE_NODE_TYPE(LRU_NODE);

LRU_CACHE_HANDLE lru_cache_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, uint32_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, int64_t capacity, LRU_CACHE_ON_ERROR_CALLBACK_FUNC on_error_callback, void* on_error_context)
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
        /*Codes_SRS_LRU_CACHE_13_010: [ If capacity is less than or equals to 0, then lru_cache_create shall fail and return NULL. ]*/
        (capacity <= 0) || 
        /*Codes_SRS_LRU_CACHE_13_079: [ If on_error_callback is NULL then lru_cache_create shall fail and return NULL. ]*/
        on_error_callback == NULL
        )
    {
        LogError("Invalid arguments: COMPUTE_HASH_FUNC compute_hash=%p, KEY_COMPARE_FUNC key_compare_func=%p, uint32_t initial_bucket_size=%u, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p, int64_t capacity=%" PRId64 ", LRU_CACHE_ON_ERROR_CALLBACK_FUNC on_error_callback=%p, void* on_error_context=%p",
            compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, capacity, on_error_callback, on_error_context);
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
        }
        else
        {
            /*Codes_SRS_LRU_CACHE_13_013: [ lru_cache_create shall assign clds_hazard_pointers to LRU_CACHE_HANDLE. ]*/
            lru_cache->clds_hazard_pointers = clds_hazard_pointers;

            /*Codes_SRS_LRU_CACHE_13_014: [ lru_cache_create shall allocate CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE by calling clds_hazard_pointers_thread_helper_create. ]*/
            lru_cache->clds_hazard_pointers_thread_helper = clds_hazard_pointers_thread_helper_create(lru_cache->clds_hazard_pointers);
            if (lru_cache->clds_hazard_pointers_thread_helper == NULL)
            {
                /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                LogError("clds_hazard_pointers_thread_helper_create failed");
            }
            else
            {
                /*Codes_SRS_LRU_CACHE_13_015: [ lru_cache_create shall allocate clds_hash_table by calling clds_hash_table_create. ]*/
                lru_cache->table = clds_hash_table_create(compute_hash, key_compare_func, initial_bucket_size, lru_cache->clds_hazard_pointers, NULL, NULL, NULL);
                if (lru_cache->table == NULL)
                {
                    /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                    LogError("Cannot create table, clds_hash_table_create failed");
                }
                else
                {
                    /*Codes_SRS_LRU_CACHE_13_016: [ lru_cache_create shall initialize SRW_LOCK_LL by calling srw_lock_ll_init. ]*/
                    if (srw_lock_ll_init(&lru_cache->srw_lock) != 0)
                    {
                        /*Codes_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
                        LogError("failure in srw_lock_ll_init(&lru_cache->srw_lock);");
                    }
                    else
                    {
                        /*Codes_SRS_LRU_CACHE_13_017: [ lru_cache_create shall initialize the doubly linked list by calling DList_InitializeListHead. ]*/
                        DList_InitializeListHead(&(lru_cache->head));

                        /*Codes_SRS_LRU_CACHE_13_018: [ lru_cache_create shall assign 0 to current_size and the capacity to capacity. ]*/
                        lru_cache->current_size = 0;
                        lru_cache->capacity = capacity;

                        lru_cache->on_error_callback = on_error_callback;
                        lru_cache->on_error_context = on_error_context;

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
        result = NULL;
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
        srw_lock_ll_deinit(&lru_cache->srw_lock);
        clds_hash_table_destroy(lru_cache->table);
        clds_hazard_pointers_thread_helper_destroy(lru_cache->clds_hazard_pointers_thread_helper);
        free(lru_cache);
    }
}

static LRU_CACHE_EVICT_RESULT evict_internal(LRU_CACHE_HANDLE lru_cache, CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread)
{
    LRU_CACHE_EVICT_RESULT result = LRU_CACHE_EVICT_OK;

    while(result == LRU_CACHE_EVICT_OK)
    {
        /*Codes_SRS_LRU_CACHE_13_040: [ lru_cache_put shall acquire the lock in exclusive. ]*/
        srw_lock_ll_acquire_exclusive(&lru_cache->srw_lock);

        int64_t current_size = interlocked_add_64(&lru_cache->current_size, 0);
        if (current_size <= lru_cache->capacity)
        {
            srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
            break;
        }
        else
        {
            /*Codes_SRS_LRU_CACHE_13_037: [ While the current size of the cache exceeds capacity: ]*/

            if (DList_IsListEmpty(&lru_cache->head))
            {
                /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                LogError("Something is wrong. The cache is empty but there is no capacity. current_size = %" PRId64 "", current_size);
                srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
                lru_cache->on_error_callback(lru_cache->on_error_context);
                result = LRU_CACHE_EVICT_ERROR;
                break;
            }

            /*Codes_SRS_LRU_CACHE_13_038: [ lru_cache_put shall get the least used node which is Flink of head node. ]*/
            DLIST_ENTRY* least_used_node = lru_cache->head.Flink;
            LRU_NODE* least_used_node_value = CONTAINING_RECORD(least_used_node, LRU_NODE, node);

            if (current_size - least_used_node_value->size < 0)
            {
                /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                LogError("current_size - least_used_node_value is less than 0. current_size=%" PRId64 ", least_used_node_value->size=%" PRId64 " Failing eviction. ", current_size, least_used_node_value->size);
                srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
                lru_cache->on_error_callback(lru_cache->on_error_context);
                result = LRU_CACHE_EVICT_ERROR;
                break;
            }
            else
            {
                /*Codes_SRS_LRU_CACHE_13_072: [ lru_cache_put shall decrement the least used node size from current_size. ]*/
                if (interlocked_compare_exchange_64(&lru_cache->current_size, current_size - least_used_node_value->size, current_size) != current_size)
                {
                    // something changed
                    srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
                    continue;
                }
                else
                {
                    CLDS_HASH_TABLE_ITEM* entry;
                    /*Codes_SRS_LRU_CACHE_13_039: [ The least used node is removed from clds_hash_table by calling clds_hash_table_remove. ]*/
                    CLDS_HASH_TABLE_REMOVE_RESULT remove_result = clds_hash_table_remove(lru_cache->table, hazard_pointers_thread, least_used_node_value->key, &entry, NULL);

                    switch (remove_result)
                    {
                    case CLDS_HASH_TABLE_REMOVE_OK:
                    {

                        /*Codes_SRS_LRU_CACHE_13_041: [ lru_cache_put shall remove the old node from the list by calling DList_RemoveEntryList. ]*/
                        (void)DList_RemoveEntryList(least_used_node);
                        LogVerbose("Removed DList entry with key=%p and size=%" PRId64 " in order to evict the lru node.", least_used_node_value->key, least_used_node_value->size);

                        /*Codes_SRS_LRU_CACHE_13_043: [ On success, evict_callback is called with the evicted item. ]*/
                        least_used_node_value->evict_callback(least_used_node_value->evict_callback_context, least_used_node_value->value);

                        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, entry);
                        break;
                    }

                    case CLDS_HASH_TABLE_REMOVE_NOT_FOUND:
                    {
                        /*Codes_SRS_LRU_CACHE_13_078: [ If clds_hash_table_remove returns CLDS_HASH_TABLE_REMOVE_NOT_FOUND, then lru_cache_put shall retry eviction. ]*/
                        LogError("item with key =%p has already been evicted.", least_used_node_value->key);
                        (void)interlocked_add_64(&lru_cache->current_size, least_used_node_value->size);
                        break;
                    }
                    default:
                    case CLDS_HASH_TABLE_REMOVE_ERROR:
                    {
                        /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                        LogError("Error removing item with key =%p from hash table", least_used_node_value->key);
                        result = LRU_CACHE_EVICT_ERROR;
                        lru_cache->on_error_callback(lru_cache->on_error_context);
                        (void)interlocked_add_64(&lru_cache->current_size, least_used_node_value->size);
                        break;
                    }
                    }
                }
            }
        }
        /*Codes_SRS_LRU_CACHE_13_042: [ lru_cache_put shall release the lock in exclusive mode. ]*/
        srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
    }

    return result;
}

static void lru_node_cleanup(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* hash_table_item)
{
    (void)context;
    LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
    if(new_node->free_func != NULL)
    {
        new_node->free_func(new_node->value);
    }
}

LRU_CACHE_PUT_RESULT lru_cache_put(LRU_CACHE_HANDLE lru_cache, void* key, void* value, int64_t size, LRU_CACHE_EVICT_CALLBACK_FUNC evict_callback, void* context, LRU_CACHE_VALUE_COPY copy_value_function, LRU_CACHE_VALUE_FREE free_value_function)
{
    LRU_CACHE_PUT_RESULT result = LRU_CACHE_PUT_OK;

    if (/*Codes_SRS_LRU_CACHE_13_023: [ If lru_handle is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        lru_cache == NULL ||
        /*Codes_SRS_LRU_CACHE_13_024: [ If key is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        key == NULL ||
        /*Codes_SRS_LRU_CACHE_13_025: [ If value is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        value == NULL ||
        /*Codes_SRS_LRU_CACHE_13_026: [ If size is less than or equals to 0, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        size <= 0 ||
        /*Codes_SRS_LRU_CACHE_13_075: [ If evict_callback is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
        evict_callback == NULL)
    {
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p, void* key=%p, CLDS_HASH_TABLE_ITEM* value=%p, int64_t size=%" PRId64 "", lru_cache, key, value, size);
        result = LRU_CACHE_PUT_ERROR;
    }
    else
    {
        /*Codes_SRS_LRU_CACHE_13_027: [ If size is greater than capacity of lru cache, then lru_cache_put shall fail and return LRU_CACHE_PUT_VALUE_INVALID_SIZE. ]*/
        if (lru_cache->capacity < size)
        {
            LogError("value size is larger than capacity.");
            result = LRU_CACHE_PUT_VALUE_INVALID_SIZE;
        }
        else
        {
            /*Codes_SRS_LRU_CACHE_13_076: [ context may be NULL. ]*/

            /*Codes_SRS_LRU_CACHE_13_028: [ lru_cache_put shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
            CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(lru_cache->clds_hazard_pointers_thread_helper);
            if (hazard_pointers_thread == NULL)
            {
                /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                LogError("clds_hazard_pointers_thread_helper_get_thread failed.");
                result = LRU_CACHE_PUT_ERROR;
            }
            else
            {
                /*Codes_SRS_LRU_CACHE_13_033: [ lru_cache_put shall acquire the lock in exclusive mode. ]*/
                srw_lock_ll_acquire_exclusive(&lru_cache->srw_lock);

                int64_t current_size = interlocked_add_64(&lru_cache->current_size, 0);
                if (INT64_MAX - size < current_size)
                {
                    /*Codes_SRS_LRU_CACHE_13_080: [ If current_size with size exceeds INT64_MAX, then lru_cache_put shall fail and return LRU_CACHE_PUT_VALUE_INVALID_SIZE. ]*/
                    LogError("Invalid sizes: Key=%p, size=%" PRId64 ", current_size = %" PRId64 ". Failing the call due to integer overflow", key, size, current_size);
                    result = LRU_CACHE_PUT_VALUE_INVALID_SIZE;
                }
                else
                {
                    /*Codes_SRS_LRU_CACHE_13_064: [ lru_cache_put shall create LRU Node item to be updated in the hash table. ]*/
                    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(LRU_NODE, lru_node_cleanup, NULL);
                    LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, item);
                    new_node->key = key;
                    new_node->size = size;
                    new_node->evict_callback = evict_callback;
                    new_node->evict_callback_context = context;

                    new_node->copy_func = copy_value_function;
                    new_node->free_func = free_value_function;
                    if(new_node->copy_func != NULL)
                    {
                        if(new_node->copy_func(&(new_node->value), value) != 0)
                        {
                            LogError("Failed to copy value. Returning with LRU_CACHE_PUT_VALUE_COPY_FUNCTION_FAILED");
                            CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, item);
                            srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
                            return LRU_CACHE_PUT_VALUE_COPY_FUNCTION_FAILED;
                        }
                    }
                    else
                    {
                        new_node->value = value;
                    }

                    CLDS_HASH_TABLE_ITEM* old_item;
                    /*Codes_SRS_LRU_CACHE_13_065: [ lru_cache_put shall update the LRU Node item in the hash table by calling clds_hash_table_set_value. ]*/
                    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(lru_cache->table, hazard_pointers_thread, key, item, NULL, NULL, &old_item, NULL);
                    if (set_value_result != CLDS_HASH_TABLE_SET_VALUE_OK)
                    {
                        /*Codes_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
                        LogError("Cannot set old key=%p to the clds_hash_table failed with error (%" PRI_MU_ENUM ")", key, MU_ENUM_VALUE(CLDS_HASH_TABLE_SET_VALUE_RESULT, set_value_result));
                        CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, item);
                        result = LRU_CACHE_PUT_ERROR;
                    }
                    else
                    {
                        if (old_item != NULL)
                        {
                            /*Codes_SRS_LRU_CACHE_13_030: [ If the key is found: ]*/
                            LRU_NODE* current_item = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, old_item);
                            PDLIST_ENTRY node = &(current_item->node);
                            /*Codes_SRS_LRU_CACHE_13_070: [ lru_cache_put shall update the current_size with the new size and removes the old value size. ]*/
                            (void)interlocked_add_64(&lru_cache->current_size, -current_item->size + size);
                            /*Codes_SRS_LRU_CACHE_13_077: [ lru_cache_put shall remove the old node from the list by calling DList_RemoveEntryList. ]*/
                            DList_RemoveEntryList(node);
                            LogVerbose("Removed DList entry with key=%p and size=%" PRId64 " in order to reposition the node.", current_item->key, current_item->size);
                            /*Codes_SRS_LRU_CACHE_13_067: [ lru_cache_put shall free the old value. ]*/
                            CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, old_item);
                        }
                        else
                        {
                            /*Codes_SRS_LRU_CACHE_13_071: [ Otherwise, if the key is not found: ]*/

                            /*Codes_SRS_LRU_CACHE_13_062: [ lru_cache_put shall add the item size to the current_size. ]*/
                            (void)interlocked_add_64(&lru_cache->current_size, size);
                        }

                        /*Codes_SRS_LRU_CACHE_13_066: [ lru_cache_put shall append the updated node to the tail to maintain the order. ]*/
                        DList_InsertTailList(&(lru_cache->head), &(new_node->node));

                        /*Codes_SRS_LRU_CACHE_13_068: [ lru_cache_put shall return with LRU_CACHE_PUT_OK. ]*/
                        result = LRU_CACHE_PUT_OK;
                    }
                }
                /*Codes_SRS_LRU_CACHE_13_036: [ lru_cache_put shall release the lock in exclusive mode. ]*/
                srw_lock_ll_release_exclusive(&lru_cache->srw_lock);

                if (result != LRU_CACHE_PUT_OK)
                {
                    LogError("Put failed for key=%p, with result (%" PRI_MU_ENUM ").", key, MU_ENUM_VALUE(LRU_CACHE_PUT_RESULT, result));
                }
                // Evict if the current size overflows capacity of the cache. 
                else if (evict_internal(lru_cache, hazard_pointers_thread) != LRU_CACHE_EVICT_OK)
                {
                    LogError("Eviction failed.");
                    result = LRU_CACHE_PUT_EVICT_ERROR;
                }
            }
        }
    }
    /*Codes_SRS_LRU_CACHE_13_049: [ On success, lru_cache_put shall return LRU_CACHE_PUT_OK. ]*/
    return result;
}


void* lru_cache_get(LRU_CACHE_HANDLE lru_cache, void* key)
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
            /*Codes_SRS_LRU_CACHE_13_056: [ lru_cache_get shall acquire the lock in exclusive mode. ]*/
            srw_lock_ll_acquire_exclusive(&lru_cache->srw_lock);

            /*Codes_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
            CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, hazard_pointers_thread, key);
            if (hash_table_item != NULL)
            {
                LRU_NODE* current_item = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
                PDLIST_ENTRY node = &(current_item->node);
                /*Codes_SRS_LRU_CACHE_13_055: [ If the key is found and the node from the key is not recently used: ]*/
                if (lru_cache->head.Blink != node)
                {
                    /*Codes_SRS_LRU_CACHE_13_057: [ lru_cache_get shall remove the old value node from doubly_linked_list by calling DList_RemoveEntryList. ]*/
                    DList_RemoveEntryList(node);
                    LogVerbose("Removed DList entry with key=%p and size=%" PRId64 " in order to reposition the node", current_item->key, current_item->size);
                    /*Codes_SRS_LRU_CACHE_13_058: [ lru_cache_get shall make the node as the tail by calling DList_InsertTailList. ]*/
                    DList_InsertTailList(&lru_cache->head, node);
                }
                result = current_item->value;
                CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, hash_table_item);
            }
            /*Codes_SRS_LRU_CACHE_13_059: [ lru_cache_get shall release the lock in exclusive mode. ]*/
            srw_lock_ll_release_exclusive(&lru_cache->srw_lock);
        }
    }

    /*Codes_SRS_LRU_CACHE_13_060: [ On success, lru_cache_get shall return CLDS_HASH_TABLE_ITEM value of the key. ]*/
    return result;
}
