
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
} LRU_NODE;

typedef void(*LRU_CACHE_EVICT_CALLBACK_FUNC)(void* context, void* evicted_value);

```

LRU Cache uses a `clds_hash_table`, a `srw_lock`, and a `doubly_linked_list`.

1. The table contains `LRU_NODE` instances as the value corresponding to the supplied keys. Each `LRU_NODE` includes a `DLIST_ENTRY` which serves as an anchor for tracking the node within the `doubly_linked_list`.

2. All operations to `clds_hash_table` and `doubly_linked_list` are performed under an exclusive lock, ensuring that changes are made together. 
This is to ensure, both `clds_hash_table` and `doubly_linked_list` are in sync with each other and also, to keep the design simple and to avoid unseen memory errors. 

### Inserting Items into the Cache

The `lru_cache_put` function is used to insert or update an item in the cache. If the item already exists in the cache, it is reinserted in the cache and moved to head position of `doubly_linked_list` to maintain the LRU order. 
If the cache is full, it performs eviction by removing the least recently used item until there is enough space for the new item.

```mermaid
graph TD
    A[Input: lru_cache, key, value, size]
    B[Acquire Lock]
    C[Set item to cache]
    D[If item exists]
    E[Update cache current size]
    F[Remove DList Entry]
    H[Add to cache current size]
    I[Add DList to tail Position]
    J[Release Lock]
    K[Eviction logic]
    L[Output: Result]

    A --> B
    B --> C
    C --> D
    D -->|Yes| E
    D -->|No| H
    E --> F
    F --> I
    H --> I
    I --> J
    J --> K
    K --> L

```

### Eviction Logic

The eviction process involves updating the cache's current size, removing the least recently used item, and invoking an eviction callback. All the latest items are inserted at the tail of the `doubly_linked_list`. 
During eviction, the node next to the head (i.e., the least recently used item) is selected and removed from the `clds_hash_table`. This is done in a loop until there is enough space in the cache. 
It's important to note that the `current_size` may temporarily increase during this process, but eviction ensures the `current_size` is normalized.

Note: As mentioned above, an exclusive lock is used when removing item from both the `clds_hash_table` and `doubly_linked_list`. 

For example: 
LRU Cache (Capacity: 2)
- put(key1, value1)
  - Head -> Node (key1)
- put(key2, value2)
  - Head -> Node (key1) -> Node (key2)
- put(key3, value3)
  - Head -> Node (key2) -> Node (key3) (`key1` is evicted)

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant HashTable as "Hash Table (Cache)"
    participant DoublyLinkedList as "Doubly Linked List"
    Cache ->> DoublyLinkedList: Acquire an exclusive lock
    DoublyLinkedList -->> Cache: Acquired lock successfully
    Cache ->> DoublyLinkedList: Find the least recently used LRU_NODE (which contains the key) in the cache (i.e., the node next to the head)
    DoublyLinkedList -->> Cache: Found the item
    Cache ->> Cache: Update the current size
    Cache ->> HashTable: Remove the key item from the cache
    HashTable -->> Cache: Item removed successfully
    Cache ->> DoublyLinkedList: Remove the node next to the head from the doubly linked list
    DoublyLinkedList -->> Cache: Item removed from the list
    Cache ->> DoublyLinkedList: Release the exclusive lock
    DoublyLinkedList -->> Cache: Released the lock successfully
    Cache ->> Cache: Call the callback provided with the eviction status
    Note left of Cache: Eviction Logic` 
```
### Getting Items from the Cache

This operation retrieves items from the cache and rearranges the order in the `doubly_linked_list` by moving the found item to its tail. It ensures that recently accessed items are placed at the tail of the list to maintain the LRU order.

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant HashTable as "Hash Table (Cache)"
    participant LRUList as "Doubly Linked List"    
    Cache ->> LRUList: Acquire an exclusive lock
    LRUList -->> Cache: Acquired lock successfully
    Cache ->> HashTable: Find the item in the hash table (key)
    HashTable -->> Cache: Found the item (hash_table_item)
    Cache ->> LRUList: Extract LRU_NODE from the item
    LRUList -->> Cache: Current item position
    LRUList ->> LRUList: Check the item's position in the list
    LRUList ->> LRUList: Move the item to the back (tail) if needed
    Cache ->> LRUList: Release the exclusive lock
    LRUList -->> Cache: Released the lock successfully
    Cache -->> Cache: Return the hash_table_item from LRU_NODE` 
```


### Scope for Improvements

- One area of improvement lies in the management of the `doubly_linked_list`, which is currently protected by a lock. To further optimize concurrent access to the cache, a lock-free `doubly_linked_list` can be used and remove `srw_lock` in its entirety. 


- Another potential enhancement involves moving `doubly_linked_list` updates to a background thread. Instead of immediately updating the list within cache operations like `get` we could use a queue for deferred processing. 
This approach enhances concurrency but may introduce race conditions between eviction and `doubly_linked_list` updates as the cache nears capacity. 
This might occasionally lead to extra waits during `put` operations, depending on whether the LRU cache must never exceed the capacity, or if it's acceptable to temporarily exceed 
(so puts can go ahead and the eviction will eventually catch up). (See this [comment](https://github.com/Azure/clds/pull/178#discussion_r1326092733) for more context)


- While `clds_singly_linked_list` handles insertions and deletions efficiently and lockless, evicting the 'tail' element can be relatively slow, as it requires traversing the list. One potential optimization is to offload this eviction process onto a separate thread. 
This could be done using `clds_singly_linked_list_find` with a callback that counts nodes until it reaches the desired eviction size and stores them in its `item_compare_context`. 
Once identified, these nodes can be efficiently removed, improving eviction performance. (See this [comment](https://github.com/Azure/clds/pull/178#discussion_r1326312429) for more context)
