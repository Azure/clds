
# LRU Cache Design

`lru_cache` is a module that implements a Least Recently Used (LRU) cache using `clds_hash_table` and `doubly_linked_list` from `c_util`.

The module provides the following functionality:

- Inserting items into the cache.
- Getting an item from the hash table by its key.
- Auto-eviction of items when the capacity is full.

All operations can be concurrent with other operations of the same or different kinds.

```c
typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread;
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

typedef void(*LRU_CACHE_EVICT_CALLBACK_FUNC)(void* context, LRU_CACHE_EVICT_RESULT cache_evict_status);
```

LRU Cache uses `clds_hash_tables`, a `srw_lock`, and a `doubly_linked_list`.

1.  The `table` stores the key and value `(void* key, CLDS_HASH_TABLE_ITEM* value)`. The `table` stores `DLIST_ENTRY` and `size` by crafting it in the form of `LRU_NODE` and storing this as a value in the form of `CLDS_HASH_TABLE_ITEM`.
2.  The `lock` is used to exclusively lock the `doubly_linked_list` while changing the order or removing the node from the list.

### Inserting Items into the Cache

The `lru_cache_put` function is used to insert or update an item in the cache. If the item already exists in the cache, it is removed and reinserted in the cache and moved to head position of `doubly_linked_list` to maintain the LRU order. If the cache is full, it performs eviction by removing the least recently used item until there is enough space for the new item.

```mermaid
graph TD
    A[Input: lru_cache, key, value, size]
    B[Acquire Lock]
    C[Find item in cache]
    D[If item exists]
    E[Remove item from cache]
    F[Eviction logic]
    G[Add item to cache]
    H[Update DList]
    I[Update cache current size]
    J[Release Lock]
    K[Output: Result]

    A --> B
    B --> C
    C --> D
    D -->|Yes| E
    D -->|No| F
    F --> G
    E --> G
    G --> H
    H --> I
    I --> J
    J --> K
```

### Eviction Logic

To evict the least recently used item from the cache, the key from the tail of the `doubly_linked_list` is removed from the `table`, and the node is truncated from the `doubly_linked_list`. When an item gets evicted, the provided callback is triggered to report the eviction status.

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant HashTable as "Hash Table (Cache)"
    participant DoublyLinkedList as "Doubly Linked List"
    Cache ->> DoublyLinkedList: Find the last LRU_NODE (which contains the key) in the cache (i.e., tail)
    DoublyLinkedList -->> Cache: Found the last item
    Cache ->> HashTable: Remove the key item from the cache
    HashTable -->> Cache: Item removed successfully
    Cache ->> DoublyLinkedList: Acquire an exclusive lock
    DoublyLinkedList -->> Cache: Acquired lock successfully
    Cache ->> DoublyLinkedList: Remove the last item from the doubly linked list
    DoublyLinkedList -->> Cache: Item removed from the list
    Cache ->> DoublyLinkedList: Release the exclusive lock
    DoublyLinkedList -->> Cache: Released the lock successfully
    Cache ->> Cache: Update the current size
    Cache ->> Cache: Call the callback provided with the eviction status
    Note left of Cache: Eviction Logic` 
```
### Getting Items from the Cache

This operation retrieves items from the cache and rearranges the order in the `doubly_linked_list` by moving the found item to its head. It ensures that recently accessed items are placed at the front of the list to maintain the LRU order.

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant HashTable as "Hash Table (Cache)"
    participant LRUList as "Doubly Linked List"
    Cache ->> HashTable: Find the item in the hash table (key)
    HashTable -->> Cache: Found the item (hash_table_item)
    Cache ->> LRUList: Extract LRU_NODE from the item
    LRUList -->> Cache: Current item position
    LRUList ->> LRUList: Check the item's position in the list
    LRUList ->> LRUList: Acquire an exclusive lock
    LRUList ->> LRUList: Move the item to the front if needed
    LRUList ->> LRUList: Release the exclusive lock
    Cache -->> Cache: Return the hash_table_item from LRU_NODE` 
```


### Scope for Improvements

One area of improvement lies in the management of the `doubly_linked_list`, which is currently protected by a lock. To further optimize concurrent access to the cache, a lock-free `doubly_linked_list` can be used and remove `srw_lock` in its entirety. 

