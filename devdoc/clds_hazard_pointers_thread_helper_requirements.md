# `clds_hazard_pointers_thread_helper` Requirements

## Overview

This is a helper which handles storing and retrieving thread-local hazard pointers threads.

## Exposed API

```c
typedef struct CLDS_HAZARD_POINTERS_THREAD_HELPER_TAG* CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE;

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, clds_hazard_pointers_thread_helper_create, CLDS_HAZARD_POINTERS_HANDLE, hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_thread_helper_destroy, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread_helper_get_thread, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);
```

Callbacks:

```c
static void clds_hazard_pointers_thread_helper_thread_notification(void* context, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);
```

### clds_hazard_pointers_thread_helper_create

```c
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, clds_hazard_pointers_thread_helper_create, CLDS_HAZARD_POINTERS_HANDLE, hazard_pointers);
```

`clds_hazard_pointers_thread_helper_create` initializes the thread-local state for using the hazard pointers and returns the handle to the helper. The `hazard_pointers` lifetime must be greater than or equal to this handle, as no reference counting is performed.

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_001: [** If `hazard_pointers` is `NULL` then `clds_hazard_pointers_thread_helper_create` shall fail and return `NULL`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_002: [** `clds_hazard_pointers_thread_helper_create` shall allocate memory for the helper. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_003: [** `clds_hazard_pointers_thread_helper_create` shall allocate the thread local storage slot for the hazard pointers by calling `TlsAlloc`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_001: [** `clds_hazard_pointers_thread_helper_create` shall obtain a `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)` by calling `thread_notifications_dispatcher_get_call_dispatcher`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_002: [** `clds_hazard_pointers_thread_helper_create` shall register `clds_hazard_pointers_thread_helper_thread_notification` call target with the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_004: [** `clds_hazard_pointers_thread_helper_create` shall succeed and return the helper. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_005: [** If there are any errors then `clds_hazard_pointers_thread_helper_create` shall fail and return `NULL`. **]**

### clds_hazard_pointers_thread_helper_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_thread_helper_destroy, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);
```

`clds_hazard_pointers_thread_helper_destroy` frees the resources from the create function.

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_006: [** If `hazard_pointers_helper` is `NULL` then `clds_hazard_pointers_thread_helper_destroy` shall return. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_004: [** `clds_hazard_pointers_thread_helper_destroy` shall unregister the call target by calling `TCALL_DISPATCHER_UNREGISTER_TARGET(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_003: [** `clds_hazard_pointers_thread_helper_destroy` shall release its reference to the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_007: [** `clds_hazard_pointers_thread_helper_destroy` shall free the thread local storage slot by calling `TlsFree`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_008: [** `clds_hazard_pointers_thread_helper_destroy` shall free the helper. **]**

### clds_hazard_pointers_thread_helper_get_thread

```c
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread_helper_get_thread, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);
```

`clds_hazard_pointers_thread_helper_get_thread` gets the `CLDS_HAZARD_POINTERS_THREAD_HANDLE` for the thread, using thread local storage to store or retrieve the handle if it has already been registered.

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_009: [** If `hazard_pointers_helper` is `NULL` then `clds_hazard_pointers_thread_helper_get_thread` shall fail and return `NULL`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_010: [** `clds_hazard_pointers_thread_helper_get_thread` shall get the thread local handle by calling `TlsGetValue`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_011: [** If no thread local handle exists then: **]**

 - **SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_012: [** `clds_hazard_pointers_thread_helper_get_thread` shall create one by calling `clds_hazard_pointers_register_thread`. **]**

 - **SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_013: [** `clds_hazard_pointers_thread_helper_get_thread` shall store the new handle by calling `TlsSetValue`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_014: [** `clds_hazard_pointers_thread_helper_get_thread` shall return the thread local handle. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_015: [** If there are any errors then `clds_hazard_pointers_thread_helper_get_thread` shall fail and return `NULL`. **]**

```c
static void clds_hazard_pointers_thread_helper_thread_notification(void* context, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);
```

`clds_hazard_pointers_thread_helper_thread_notification` is the handler of the thread notifications callback.

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_005: [** If `context` is `NULL`, `clds_hazard_pointers_thread_helper_thread_notification` shall terminate the process. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_006: [** If reason is `THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH`, `clds_hazard_pointers_thread_helper_thread_notification` shall return. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_007: [** If reason is `THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH`, `clds_hazard_pointers_thread_helper_thread_notification` shall call `TlsGetValue` obtain the thread local value for the slot created in the `clds_hazard_pointers_thread_create`. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_008: [** If the thread local stored value is `NULL`, `clds_hazard_pointers_thread_helper_thread_notification` shall return. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_009: [** If the thread local stored value is not `NULL`: **]**

  - **SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_010: [** `clds_hazard_pointers_thread_helper_thread_notification` shall set the value in the slot to `NULL` by calling `TlsSetValue`. **]**

  - **SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_011: [** `clds_hazard_pointers_thread_helper_thread_notification` shall call `clds_hazard_pointers_unregister_thread` with the argument being the obtained thread local value. **]**

**SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_012: [** If `reason` is any other value, `clds_hazard_pointers_thread_helper_thread_notification` shall return. **]**