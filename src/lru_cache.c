// Licensed under the MIT license.See LICENSE file in the project root for full license information.

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

#include "c_util/doublylinkedlist.h"

#include "clds/clds_singly_linked_list.h"
#include "clds/clds_hazard_pointers.h"

#include "clds/lru_cache.h"

typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread;
    CLDS_HASH_TABLE_HANDLE table;
    CLDS_HASH_TABLE_HANDLE link_table;

    int64_t current_size;
    int64_t capacity;

    DWORD tls_slot;
    volatile_atomic int64_t* seq_no;

    DLIST_ENTRY head;
} LRU_CACHE;

typedef struct LRU_NODE_TAG
{
    void* key;
    int64_t size;
    DLIST_ENTRY node;
} LRU_NODE;
DECLARE_HASH_TABLE_NODE_TYPE(LRU_NODE);

LRU_CACHE_HANDLE lru_cache_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context, int64_t capacity)
{
    LRU_CACHE_HANDLE lru_cache;

    if (
        (compute_hash == NULL) ||
        (key_compare_func == NULL) ||
        (initial_bucket_size == 0) ||
        (clds_hazard_pointers == NULL) ||
        ((start_sequence_number == NULL) && (skipped_seq_no_cb != NULL))
        )
    {
        LogError("Invalid arguments: COMPUTE_HASH_FUNC compute_hash=%p, KEY_COMPARE_FUNC key_compare_func=%p, size_t initial_bucket_size=%zu, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers=%p, volatile_atomic int64_t* start_sequence_number=%p, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb=%p, void* skipped_seq_no_cb_context=%p",
            compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
        lru_cache = NULL;
    }
    else
    {

        lru_cache = malloc(sizeof(lru_cache));
        if (lru_cache == NULL)
        {
            LogError("Cannot allocate memory for lru_cache");
        }
        else
        {
            lru_cache->tls_slot = TlsAlloc();
            lru_cache->clds_hazard_pointers = clds_hazard_pointers_create();

            lru_cache->hazard_pointers_thread = clds_hazard_pointers_register_thread(lru_cache->clds_hazard_pointers);

            lru_cache->table = clds_hash_table_create(compute_hash, key_compare_func, 1024 * 1024, lru_cache->clds_hazard_pointers, lru_cache->seq_no, skipped_seq_no_cb, skipped_seq_no_cb_context);

            lru_cache->link_table = clds_hash_table_create(compute_hash, key_compare_func, 1024 * 1024, lru_cache->clds_hazard_pointers, lru_cache->seq_no, skipped_seq_no_cb, skipped_seq_no_cb_context);

            lru_cache->current_size = 0;
            lru_cache->capacity = capacity;
            DList_InitializeListHead(&(lru_cache->head));

        }
    }
    return lru_cache;
}

void lru_cache_destroy(LRU_CACHE_HANDLE lru_cache)
{
    if (lru_cache == NULL)
    {
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p", lru_cache);
    }
    else
    {
        clds_hash_table_destroy(lru_cache->table);
        clds_hazard_pointers_destroy(lru_cache->clds_hazard_pointers);
        TlsFree(lru_cache->tls_slot);
    }
}


int lru_cache_put(LRU_CACHE_HANDLE lru_cache, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t size)
{
    CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, lru_cache->hazard_pointers_thread, key);

    if (hash_table_item != NULL)
    {
        CLDS_HASH_TABLE_ITEM* link_node_item = clds_hash_table_find(lru_cache->link_table, lru_cache->hazard_pointers_thread, key);
        LRU_NODE* doubly_value = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, link_node_item);
        DLIST_ENTRY node = doubly_value->node;

        // Update the key and value (value could have been changed) 

        if (node.Flink == lru_cache->head.Flink)
        {
            
        }
        else
        {
            // srw lock needs to come
            DList_RemoveEntryList(&node);
            DList_InsertHeadList(&lru_cache->head, &node);
        }
    }
    else
    {
        // eviction logic. Should move to internal_evict function
        while (lru_cache->current_size + size >= lru_cache->capacity)
        {
            // evict the last element
            DLIST_ENTRY* last_node = lru_cache->head.Blink;
            LRU_NODE* last_node_value = (LRU_NODE*)CONTAINING_RECORD(last_node, LRU_NODE, node);
            int64_t seq_no;

            //remove from clds_hash_table
            CLDS_HASH_TABLE_ITEM* removed_item;
            CLDS_HASH_TABLE_ITEM* removed_link;

            // Need to delete from DLinkedList
            CLDS_HASH_TABLE_REMOVE_RESULT res = clds_hash_table_remove(lru_cache->table, lru_cache->hazard_pointers_thread, last_node_value->key, &removed_item, &seq_no);
            res = clds_hash_table_remove(lru_cache->link_table, lru_cache->hazard_pointers_thread, last_node_value->key, &removed_link, &seq_no);

            if (res != CLDS_HASH_TABLE_REMOVE_OK)
            {
                LogError("Error removing item from hash table");
            }
            else
            {
                //remove from doubly linked list
                lru_cache->current_size -= last_node_value->size;
                DList_RemoveEntryList(last_node);
            }
        }
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(LRU_NODE, NULL, NULL);
        LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, item);
        new_node->key = key;
        new_node->size = size;

        CLDS_HASH_TABLE_NODE_INC_REF(LRU_NODE, item);

        DList_InsertHeadList(&lru_cache->head, &new_node->node);

        int64_t insert_seq_no;
        CLDS_HASH_TABLE_INSERT_RESULT hash_table_insert = clds_hash_table_insert(lru_cache->table, lru_cache->hazard_pointers_thread, key, value, &insert_seq_no);
        hash_table_insert = clds_hash_table_insert(lru_cache->link_table, lru_cache->hazard_pointers_thread, key, item, &insert_seq_no);
        
        if (hash_table_insert == CLDS_HASH_TABLE_INSERT_ERROR)
        {
            // Nothing
        }
        else
        {
            lru_cache->current_size += size;
        }

    }
    return 0;
}

CLDS_HASH_TABLE_ITEM* lru_cache_get(LRU_CACHE_HANDLE lru_cache, void* key)
{
    CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, lru_cache->hazard_pointers_thread, key);

    if (hash_table_item != NULL)
    {
        CLDS_HASH_TABLE_ITEM* link_item = clds_hash_table_find(lru_cache->link_table, lru_cache->hazard_pointers_thread, key);

        LRU_NODE* doubly_value = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, link_item);
        DLIST_ENTRY node = doubly_value->node;
        if (node.Flink == lru_cache->head.Flink)
        {
            // Lets not do anything
        }
        else
        {
            DList_RemoveEntryList(&node);
            DList_InsertHeadList(&lru_cache->head, &node);
        }
    }

    return hash_table_item;
}