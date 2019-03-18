`lock_free_set` requirements
================

## Overview

`lock_free_set` is a module that implements a lock free set. The set allows thread safe inserts and removes.
It also provides a purge functionality which is not intended to be thread safe.

## Exposed API

```C
typedef struct LOCK_FREE_SET_TAG* LOCK_FREE_SET_HANDLE;

typedef volatile struct LOCK_FREE_SET_ITEM_TAG
{
    volatile struct LOCK_FREE_SET_ITEM_TAG* previous;
    volatile struct LOCK_FREE_SET_ITEM_TAG* next;
} LOCK_FREE_SET_ITEM;

typedef void(*NODE_CLEANUP_FUNC)(void* context, LOCK_FREE_SET_ITEM* item);

MOCKABLE_FUNCTION(, LOCK_FREE_SET_HANDLE, lock_free_set_create);
MOCKABLE_FUNCTION(, void, lock_free_set_destroy, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);
MOCKABLE_FUNCTION(, int, lock_free_set_insert, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
MOCKABLE_FUNCTION(, int, lock_free_set_remove, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
MOCKABLE_FUNCTION(, int, lock_free_set_purge_not_thread_safe, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);
```

### lock_free_set_create

```C
MOCKABLE_FUNCTION(, LOCK_FREE_SET_HANDLE, lock_free_set_create);
```

**SRS_LOCK_FREE_SET_01_001: [** `lock_free_set_create` shall create a lock free set. **]**

**SRS_LOCK_FREE_SET_01_002: [** On success, `lock_free_set_create` shall return a non-NULL handle to the newly created set. **]**

**SRS_LOCK_FREE_SET_01_003: [** If `lock_free_set_create` fails allocating memory for the set then it shall fail and return NULL. **]**

### lock_free_set_destroy

```C
MOCKABLE_FUNCTION(, void, lock_free_set_destroy, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);
```

**SRS_LOCK_FREE_SET_01_004: [** `lock_free_set_destroy` shall free all resources associated with the lock free set. **]**

**SRS_LOCK_FREE_SET_01_005: [** If `lock_free_set` is NULL, `lock_free_set_destroy` shall do nothing. **]**

**SRS_LOCK_FREE_SET_01_006: [** `node_cleanup_callback` and `context` shall be allowed to be NULL. **]**

**SRS_LOCK_FREE_SET_01_007: [** If `node_cleanup_callback` is non-NULL, `node_cleanup_callback` shall be called for each item that exists in the set at the time `lock_free_set_destroy` is called. **]**

**SRS_LOCK_FREE_SET_01_008: [** When `node_cleanup_callback` is called the set item pointer and `context` shall be passed as arguments. **]**

### lock_free_set_insert

```C
MOCKABLE_FUNCTION(, int, lock_free_set_insert, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
```

**SRS_LOCK_FREE_SET_01_009: [** `lock_free_set_insert` shall insert the item `item` in the set. **]**

**SRS_LOCK_FREE_SET_01_010: [** On success it shall return 0. **]**

**SRS_LOCK_FREE_SET_01_011: [** If `lock_free_set` or `item` is NULL, `lock_free_set_insert` shall fail and return a non-zero value. **]**

### lock_free_set_remove

```C
MOCKABLE_FUNCTION(, int, lock_free_set_remove, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
```

**SRS_LOCK_FREE_SET_01_015: [** `lock_free_set_remove` shall remove the item `item` from the set. **]**

**SRS_LOCK_FREE_SET_01_016: [** On success it shall return 0. **]**

**SRS_LOCK_FREE_SET_01_017: [** If `lock_free_set` or `item` is NULL, `lock_free_set_remove` shall fail and return a non-zero value. **]**

### lock_free_set_purge_not_thread_safe

```C
MOCKABLE_FUNCTION(, int, lock_free_set_purge_not_thread_safe, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);
```

**SRS_LOCK_FREE_SET_01_019: [** `lock_free_set_purge_not_thread_safe` shall remove all items in the set. **]**

**SRS_LOCK_FREE_SET_01_020: [** On success it shall return 0. **]**

**SRS_LOCK_FREE_SET_01_021: [** If `lock_free_set` is NULL, `lock_free_set_purge_not_thread_safe` shall fail and return a non-zero value. **]**

**SRS_LOCK_FREE_SET_01_022: [** `node_cleanup_callback` and `context` shall be allowed to be NULL. **]**

**SRS_LOCK_FREE_SET_01_023: [** If `node_cleanup_callback` is non-NULL, `node_cleanup_callback` shall be called for each item that exists in the set at the time `lock_free_set_purge_not_thread_safe` is called. **]**

**SRS_LOCK_FREE_SET_01_024: [** When `node_cleanup_callback` is called the set item pointer and `context` shall be passed as arguments. **]**

### Thread safety

`lock_free_set_purge_not_thread_safe` and `lock_free_set_destroy` come with no thread safety guarantees, the user of the module must ensure that these calls are made in a thread safe way.

**SRS_LOCK_FREE_SET_01_013: [** `lock_free_set_insert` and `lock_free_set_remove` shall be safe to be called from multiple threads. **]**

