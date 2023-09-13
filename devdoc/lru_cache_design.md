
# LRU Cache Design

`lru_cache` is a module that implements a Least Recently Used (LRU) cache using `clds_hash_table` and `doublylinkedlist` from `c_util`.

The module provides the following functionality:

- Inserting items in the cache.
- Getting an item in the hash table by its key.
- Evicting the least recently used item and auto-eviction of items when capacity is full.

All operations can be concurrent with other operations of the same or different kinds.

```c
typedef struct LRU_CACHE_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread;
    CLDS_HASH_TABLE_HANDLE item_table;
    CLDS_HASH_TABLE_HANDLE link_table;
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
    DLIST_ENTRY node;
} LRU_NODE;
```

LRU Cache uses 2 `clds_hash_tables`, a `srw_lock`, and a `doubly_linked_list`.
1. `item_table` stores the key and values `(void* key, CLDS_HASH_TABLE_ITEM* value)`.
2. `link_table` stores the key and `doubly_linked_list` node `(void* key, LRU_NODE value)`. This is used to quickly access the node and to maintain the LRU order.
3. `lock` is used to exclusively lock the `doubly_linked_list` while changing the order or removing the node from the list.

### Inserting items into the cache

The `lru_cache_put` function is used to insert or update an item in the cache. If the item already exists in the cache, it is removed and reinserted to maintain the LRU order. If the cache is full, it performs eviction by removing the least recently used item until there is enough space for the new item.

```mermaid
graph TD
    A[Input: lru_cache, key, value, size]
    B[Acquire Lock]
    C[Find item in cache]
    D[If item exists]
    E[Remove item from cache]
    F[Eviction logic]
    G[Add item to cache]
    H[Update cache current size]
    I[Release Lock]
    J[Output: Result]

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
```

### Eviction

To evict the least recently used item from the cache, the key from the tail of the `doubly_linked_list` is removed in both the tables (i.e., `link_table` and `item_table`), and the node is truncated from the `doubly_linked_list`.

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant HashTable as "Main Hash Table"
    participant LinkHashTable as "Link Table"
    participant DoublyLinkedList as "Doubly Linked List"
    Cache ->> DoublyLinkedList: Find last LRU_NODE (which contains the key) in the cache (i.e., tail)
    DoublyLinkedList -->> Cache: Found the last item
    Cache ->> HashTable: Remove the key item from the cache
    HashTable -->> Cache: Item removed successfully
    Cache ->> LinkHashTable: Remove the key item from the link table
    LinkHashTable -->> Cache: Item removed successfully
    Cache ->> DoublyLinkedList: Acquire exclusive lock
    DoublyLinkedList -->> Cache: Acquired lock successfully
    Cache ->> DoublyLinkedList: Remove the last item from the doubly linked list
    DoublyLinkedList -->> Cache: Item removed from the list
    Cache ->> DoublyLinkedList: Release the exclusive lock
    DoublyLinkedList -->> Cache: Released the lock successfully
    Cache ->> Cache: Update the current size
    Note left of Cache: Eviction Logic
```

### Getting items from the cache

This operation retrieves items from the cache and rearranges the order in the `doubly_linked_list` by moving the found item to its head. It ensures that recently accessed items are placed at the front of the list to maintain the LRU order.

```mermaid
sequenceDiagram
    participant Cache as "LRU Cache"
    participant MainHashTable as "Main Hash Table"
    participant LinkHashTable as "Linked Hash Table"
    participant LRUList as "Doubly Linked List"
    Cache ->> MainHashTable: Find the item in the main hash table (key)
    MainHashTable -->> Cache: Found the item (hash_table_item)
    Cache ->> LinkHashTable: Find the item in the linked hash table (key)
    LinkHashTable -->> Cache: Found the item (link_item)
    Cache ->> LRUList: Extract LRU_NODE from link_item
    LRUList ->> LRUList: Check the item's position in the list
    LRUList -->> Cache: Current item position
    LRUList ->> LRUList: Acquire an exclusive lock
    LRUList ->> LRUList: Move the item to the front if needed
    LRUList ->> LRUList: Release the exclusive lock
    Cache -->> Cache: Return hash_table_item
```
