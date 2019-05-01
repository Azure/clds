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

**S_R_S_CLDS_HAZARD_POINTERS_01_001: [** `clds_hazard_pointers_create` shall create a new hazard pointers instance and on success return a non-NULL handle to it. **]**

**S_R_S_CLDS_HAZARD_POINTERS_01_002: [** If any error happens, `clds_hazard_pointers_create` shall fail and return NULL. **]**

**S_R_S_CLDS_HAZARD_POINTERS_01_003: [** `reclaim_func` shall be saved for later use when reclaiming nodes. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_destroy, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**S_R_S_CLDS_HAZARD_POINTERS_01_004: [** `clds_hazard_pointers_destroy` shall free all resources associated with the hazard pointers instance. **]**

**S_R_S_CLDS_HAZARD_POINTERS_01_005: [** If `clds_hazard_pointers` is NULL, `clds_hazard_pointers_destroy` shall return. **]**
