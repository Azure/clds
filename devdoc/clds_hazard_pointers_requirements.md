# `clds_hazard_pointers` requirements

## Overview

`clds_hazard_pointers` is module that implements hazard pointers that can be used for building lockless data structures.
The module attempts to follow as much as possible the paper by Maged M. Michael (http://www.research.ibm.com/people/m/michael/ieeetpds-2004.pdf).

## Exposed API

```c
typedef struct CLDS_HAZARD_POINTERS_TAG* CLDS_HAZARD_POINTERS_HANDLE;
typedef struct CLDS_HAZARD_POINTERS_THREAD_TAG* CLDS_HAZARD_POINTERS_THREAD_HANDLE;
typedef struct CLDS_HAZARD_POINTER_RECORD_TAG* CLDS_HAZARD_POINTER_RECORD_HANDLE;

typedef void(*RECLAIM_FUNC)(void* node);

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers_create, RECLAIM_FUNC, reclaim_func);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_destroy, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_register_thread, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_unregister_thread, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointers_acquire, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, node);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_release, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointer_record);
```

### clds_hazard_pointers_create

```c
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers_create, RECLAIM_FUNC, reclaim_func);
```

**SRS_CLDS_HAZARD_POINTERS_01_001: [** `clds_hazard_pointers_create` shall create a new hazard pointers instance and on success return a non-NULL handle to it. **]**

**SRS_CLDS_HAZARD_POINTERS_01_002: [** If any error happens, `clds_hazard_pointers_create` shall fail and return NULL. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_destroy, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**SRS_CLDS_HAZARD_POINTERS_01_004: [** `clds_hazard_pointers_destroy` shall free all resources associated with the hazard pointers instance. **]**

**SRS_CLDS_HAZARD_POINTERS_01_005: [** If `clds_hazard_pointers` is NULL, `clds_hazard_pointers_destroy` shall return. **]**

### clds_hazard_pointers_register_thread

```c
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_register_thread, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**SRS_CLDS_HAZARD_POINTERS_01_006: [** `clds_hazard_pointers_register_thread` shall register the current thread with the hazard pointers instance `clds_hazard_pointers` and on success return a non-NULL handle to the registered thread. **]**

**SRS_CLDS_HAZARD_POINTERS_01_007: [** If `clds_hazard_pointers` is NULL, `clds_hazard_pointers_register_thread` shall fail and return NULL. **]**

**SRS_CLDS_HAZARD_POINTERS_01_008: [** If any error occurs, `clds_hazard_pointers_register_thread` shall fail and return NULL. **]**

### clds_hazard_pointers_unregister_thread

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_unregister_thread, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```

**SRS_CLDS_HAZARD_POINTERS_01_009: [** `clds_hazard_pointers_unregister_thread` shall unregister the thread identified by `clds_hazard_pointers_thread` from its associated hazard pointers instance. **]**

**SRS_CLDS_HAZARD_POINTERS_01_010: [** If `clds_hazard_pointers_thread` is NULL, `clds_hazard_pointers_unregister_thread` shall return. **]**

### clds_hazard_pointers_acquire

```c
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointers_acquire, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, node);
```

**SRS_CLDS_HAZARD_POINTERS_01_011: [** `clds_hazard_pointers_acquire` shall acquire a hazard pointer for the given `node` and on success return a non-NULL handle to the hazard pointer record. **]**

**SRS_CLDS_HAZARD_POINTERS_01_012: [** If `clds_hazard_pointers_thread` is NULL, `clds_hazard_pointers_acquire` shall fail and return NULL. **]**

**SRS_CLDS_HAZARD_POINTERS_01_013: [** If any error occurs, `clds_hazard_pointers_acquire` shall fail and return NULL. **]**

### clds_hazard_pointers_release

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_release, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointer_record);
```

**SRS_CLDS_HAZARD_POINTERS_01_014: [** `clds_hazard_pointers_release` shall release the hazard pointer associated with `clds_hazard_pointer_record`. **]**

**SRS_CLDS_HAZARD_POINTERS_01_015: [** If `clds_hazard_pointer_record` is NULL, `clds_hazard_pointers_release` shall return. **]**

### clds_hazard_pointers_reclaim

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_reclaim, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, node, RECLAIM_FUNC, reclaim_func);
```

**SRS_CLDS_HAZARD_POINTERS_01_016: [** `clds_hazard_pointers_reclaim` shall add the `node` to the reclaim list and when the reclaim threshold is reached, it shall trigger a reclaim cycle. **]**

**SRS_CLDS_HAZARD_POINTERS_01_017: [** If `clds_hazard_pointers_thread` is NULL, `clds_hazard_pointers_reclaim` shall return. **]**

**SRS_CLDS_HAZARD_POINTERS_01_018: [** If `node` is NULL, `clds_hazard_pointers_reclaim` shall return. **]**

**SRS_CLDS_HAZARD_POINTERS_01_019: [** If any other thread has acquired a hazard pointer for `node`, the `reclaim_func` shall not be called for `node`. **]**

**SRS_CLDS_HAZARD_POINTERS_01_020: [** If no thread has acquired a hazard pointer for `node`, `reclaim_func` shall be called for `node` to reclaim it. **]**

### clds_hazard_pointers_set_reclaim_threshold

```c
MOCKABLE_FUNCTION(, int, clds_hazard_pointers_set_reclaim_threshold, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers, size_t, reclaim_threshold);
```

**SRS_CLDS_HAZARD_POINTERS_01_021: [** `clds_hazard_pointers_set_reclaim_threshold` shall set the reclaim threshold for the hazard pointers instance to `reclaim_threshold`. **]**

**SRS_CLDS_HAZARD_POINTERS_01_022: [** If `clds_hazard_pointers` is NULL, `clds_hazard_pointers_set_reclaim_threshold` shall fail and return a non-zero value. **]**

**SRS_CLDS_HAZARD_POINTERS_01_023: [** If `reclaim_threshold` is 0, `clds_hazard_pointers_set_reclaim_threshold` shall fail and return a non-zero value. **]**

**SRS_CLDS_HAZARD_POINTERS_01_024: [** On success, `clds_hazard_pointers_set_reclaim_threshold` shall return 0. **]**
