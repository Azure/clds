`mpsc_lock_free_queue` requirements
================

## Overview

`mpsc_lock_free_queue` is a module that implements a multiple producer, single consumer lock free queue.
Multiple concurrent enqueues are thread safe. Also the dequeue can be done concurrently with the enqueues in a thread safe way.
Multiple concurrent dequeues are not thread safe.

## Exposed API

```c
typedef struct MPSC_LOCK_FREE_QUEUE_TAG* MPSC_LOCK_FREE_QUEUE_HANDLE;

typedef volatile struct MPSC_LOCK_FREE_QUEUE_ITEM_TAG
{
    volatile struct MPSC_LOCK_FREE_QUEUE_ITEM_TAG* next;
} MPSC_LOCK_FREE_QUEUE_ITEM;

MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue_create);
MOCKABLE_FUNCTION(, void, mpsc_lock_free_queue_destroy, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_enqueue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, MPSC_LOCK_FREE_QUEUE_ITEM*, item);
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_dequeue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_peek, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_is_empty, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, bool*, is_empty);
```

### mpsc_lock_free_queue_create

```c
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue_create);
```

**SRS_MPSC_LOCK_FREE_QUEUE_01_001: [** `mpsc_lock_free_queue_create` shall create a lock free multiple producer single consumer queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_002: [** On success, `mpsc_lock_free_queue_create` shall return a non-NULL handle to the newly created queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_003: [** If `mpsc_lock_free_queue_create` fails allocating memory for the queue then it shall fail and return NULL. **]**

### mpsc_lock_free_queue_destroy

```c
MOCKABLE_FUNCTION(, void, mpsc_lock_free_queue_destroy, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
```

**SRS_MPSC_LOCK_FREE_QUEUE_01_004: [** `mpsc_lock_free_queue_destroy` shall free all resources associated with the lock free queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_005: [** If `mpsc_lock_free_queue` is NULL, `mpsc_lock_free_queue_destroy` shall do nothing. **]**

### mpsc_lock_free_queue_enqueue

```c
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_enqueue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, MPSC_LOCK_FREE_QUEUE_ITEM*, item);
```

**SRS_MPSC_LOCK_FREE_QUEUE_01_006: [** `mpsc_lock_free_queue_enqueue` shall enqueue the item `item` in the queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_007: [** On success it shall return 0. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_008: [** If `mpsc_lock_free_queue` or `item` is NULL, `mpsc_lock_free_queue_enqueue` shall fail and return a non-zero value. **]**

### mpsc_lock_free_queue_dequeue

```c
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_dequeue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
```

**SRS_MPSC_LOCK_FREE_QUEUE_01_009: [** `mpsc_lock_free_queue_dequeue` shall dequeue the oldest item in the queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_010: [** On success it shall return a pointer to the dequeued item. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_011: [** If `mpsc_lock_free_queue` is NULL, `mpsc_lock_free_queue_dequeue` shall fail and return NULL. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_019: [** If the queue is empty, `mpsc_lock_free_queue_dequeue` shall return NULL. **]**

### mpsc_lock_free_queue_peek

```c
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_peek, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
```

XX**SRS_MPSC_LOCK_FREE_QUEUE_01_025: [** `mpsc_lock_free_queue_peek` shall return the oldest item in the queue without removing it. **]**

XX**SRS_MPSC_LOCK_FREE_QUEUE_01_026: [** On success it shall return a non-NULL pointer to the peeked item. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_027: [** If `mpsc_lock_free_queue` is NULL, `mpsc_lock_free_queue_peek` shall fail and return NULL. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_028: [** If the queue is empty, `mpsc_lock_free_queue_peek` shall return NULL. **]**

### mpsc_lock_free_queue_is_empty

```c
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_is_empty, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, bool*, is_empty);
```

**SRS_MPSC_LOCK_FREE_QUEUE_01_021: [** `mpsc_lock_free_queue_is_empty` shall set `is_empty` to false if at least one item is in the queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_022: [** `mpsc_lock_free_queue_is_empty` shall set `is_empty` to true if no items are in the queue. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_023: [** On success it shall return 0. **]**

**SRS_MPSC_LOCK_FREE_QUEUE_01_024: [** If `mpsc_lock_free_queue` or `is_empty` is NULL then `mpsc_lock_free_queue_is_empty` shall fail and return a non-zero value. **]**

### Thread safety

It is assumed that the `mpsc_lock_free_queue_is_empty`, `mpsc_lock_free_queue_peek`, `mpsc_lock_free_queue_dequeue` are not safe to be called concurrently (they should be used from one thread only).

**SRS_MPSC_LOCK_FREE_QUEUE_01_014: [** Concurrent `mpsc_lock_free_queue_enqueue` from multiple threads shall be safe. **]**

