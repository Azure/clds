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
#include "c_pal/srw_lock.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hazard_pointers.h"

#include "clds/lru_cache.h"


typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;

    CLDS_HASH_TABLE_HANDLE table;

    int64_t current_size;
    int64_t capacity;

    DWORD tls_slot;
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

LRU_CACHE_HANDLE lru_cache_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context, int64_t capacity)
{
    LRU_CACHE_HANDLE result;

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
        result = NULL;
    }
    else
    {

        LRU_CACHE_HANDLE lru_cache = malloc(sizeof(LRU_CACHE));
        if (lru_cache == NULL)
        {
            LogError("malloc failed for LRU_CACHE_HANDLE");
            result = NULL;
        }
        else
        {
            lru_cache->seq_no = start_sequence_number;

            lru_cache->tls_slot = TlsAlloc();
            if (lru_cache->tls_slot == TLS_OUT_OF_INDEXES)
            {
                LogError("Cannot allocate Tls slot");
                result = NULL;
            }
            else
            {
                lru_cache->clds_hazard_pointers = clds_hazard_pointers_create();
                if (lru_cache->clds_hazard_pointers == NULL)
                {
                    LogError("Cannot create clds hazard pointers");
                    result = NULL;
                }
                else
                {
                    lru_cache->table = clds_hash_table_create(compute_hash, key_compare_func, initial_bucket_size, lru_cache->clds_hazard_pointers, lru_cache->seq_no, skipped_seq_no_cb, skipped_seq_no_cb_context);

                    if (lru_cache->table == NULL)
                    {
                        LogError("Cannot create table, clds_hash_table_create failed");
                        result = NULL;
                    }
                    else
                    {
                        lru_cache->lock = srw_lock_create(false, "lru_cache_lock");
                        
                        if (lru_cache->lock == NULL)
                        {
                            LogError("failure in srw_lock_create(false, \"lru_cache_lock\")");
                            result = NULL;
                        }
                        else
                        {
                            DList_InitializeListHead(&(lru_cache->head));

                            lru_cache->current_size = 0;
                            lru_cache->capacity = capacity;

                            result = lru_cache;
                            goto all_ok;
                        }
                        clds_hash_table_destroy(lru_cache->table);
                    }
                    clds_hazard_pointers_destroy(lru_cache->clds_hazard_pointers);
                }
                TlsFree(lru_cache->tls_slot);
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
        LogError("Invalid arguments: LRU_CACHE_HANDLE lru_cache=%p", lru_cache);
    }
    else
    {
        srw_lock_destroy(lru_cache->lock);
        clds_hash_table_destroy(lru_cache->table);
        clds_hazard_pointers_destroy(lru_cache->clds_hazard_pointers);
        TlsFree(lru_cache->tls_slot);
        free(lru_cache);
    }
}

static CLDS_HAZARD_POINTERS_THREAD_HANDLE get_hazard_pointers_thread(LRU_CACHE_HANDLE lru_cache)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = TlsGetValue(lru_cache->tls_slot);
    if (hazard_pointers_thread == NULL)
    {
        hazard_pointers_thread = clds_hazard_pointers_register_thread(lru_cache->clds_hazard_pointers);
        if (hazard_pointers_thread == NULL)
        {
            LogError("Cannot create clds hazard pointers thread");
        }
        else
        {
            if (!TlsSetValue(lru_cache->tls_slot, hazard_pointers_thread))
            {
                LogError("Cannot set Tls slot value");
            }
            else
            {
                goto all_ok;
            }
            clds_hazard_pointers_unregister_thread(hazard_pointers_thread);
            hazard_pointers_thread = NULL;
        }
    }

all_ok:
    return hazard_pointers_thread;
}

int add_to_cache_internal(LRU_CACHE_HANDLE lru_cache, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t size)
{
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(LRU_NODE, NULL, NULL);
    LRU_NODE* new_node = CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, item);
    new_node->key = key;
    new_node->size = size;
    new_node->value = value; 

    int64_t insert_seq_no = 0;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = get_hazard_pointers_thread(lru_cache);

    CLDS_HASH_TABLE_INSERT_RESULT hash_table_insert = clds_hash_table_insert(lru_cache->table, hazard_pointers_thread, key, item, &insert_seq_no); 

    if (hash_table_insert == CLDS_HASH_TABLE_INSERT_ERROR)
    {
        return 1;
    }
    else
    {
        srw_lock_acquire_exclusive(lru_cache->lock);
        {
            DList_InsertHeadList(&lru_cache->head, &new_node->node);
            lru_cache->current_size += size;
        }
        srw_lock_release_exclusive(lru_cache->lock);
    }
    return 0;
}

int lru_cache_put(LRU_CACHE_HANDLE lru_cache, void* key, CLDS_HASH_TABLE_ITEM* value, int64_t size, int64_t seq_no)
{
    int result = 0;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = get_hazard_pointers_thread(lru_cache);

    CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, hazard_pointers_thread, key);

    if (hash_table_item != NULL)
    {
        LRU_NODE* doubly_value = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
        DLIST_ENTRY node = doubly_value->node;

        CLDS_HASH_TABLE_ITEM* entry;
        (void)clds_hash_table_remove(lru_cache->table, hazard_pointers_thread, key, &entry, &seq_no);

        srw_lock_acquire_exclusive(lru_cache->lock);
        {
            lru_cache->current_size -= doubly_value->size;
            DList_RemoveEntryList(&node);
        }
        srw_lock_release_exclusive(lru_cache->lock);
    }
    else
    {
        while ((lru_cache->current_size + size >= lru_cache->capacity))
        {
            DLIST_ENTRY* last_node = lru_cache->head.Blink;
            LRU_NODE* last_node_value = (LRU_NODE*)CONTAINING_RECORD(last_node, LRU_NODE, node);
            CLDS_HASH_TABLE_ITEM* entry;

            CLDS_HASH_TABLE_REMOVE_RESULT res = clds_hash_table_remove(lru_cache->table, hazard_pointers_thread, last_node_value->key, &entry, &seq_no);

            if (res != CLDS_HASH_TABLE_REMOVE_OK)
            {
                LogError("Error removing item from hash table");
            }
            else
            {
                srw_lock_acquire_exclusive(lru_cache->lock);
                {
                    lru_cache->current_size -= last_node_value->size;
                    DList_RemoveEntryList(last_node);
                    CLDS_HASH_TABLE_NODE_RELEASE(LRU_NODE, entry);
                }
                srw_lock_release_exclusive(lru_cache->lock);
            }
        }
    }

    result = add_to_cache_internal(lru_cache, key, value, size);

    return result;
}


CLDS_HASH_TABLE_ITEM* lru_cache_get(LRU_CACHE_HANDLE lru_cache, void* key)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = get_hazard_pointers_thread(lru_cache);

    CLDS_HASH_TABLE_ITEM* hash_table_item = clds_hash_table_find(lru_cache->table, hazard_pointers_thread, key);

    if (hash_table_item != NULL)
    {
        LRU_NODE* doubly_value = (LRU_NODE*)CLDS_HASH_TABLE_GET_VALUE(LRU_NODE, hash_table_item);
        DLIST_ENTRY node = doubly_value->node;

        srw_lock_acquire_exclusive(lru_cache->lock);
        {
            DList_RemoveEntryList(&node);
            DList_InsertHeadList(&lru_cache->head, &node);
        }
        srw_lock_release_exclusive(lru_cache->lock);

        return doubly_value->value;
    }

    return hash_table_item;
}