# `thread_notifications_dispatcher` requirements

## Overview

`thread_notifications_dispatcher` is a module that implements a singleton instance of `TCALL_DISPATCHER` for thread notifications (attach/detach).

`thread_notifications_dispatcher` initialization/deinitialization is explicit and not thread safe (no other APIs should be called while init/deinit executes).

It is intended to be called at a global level for the system (one of the first things in `main` for `init`, one of the last things in `main` for `deinit`), for example it is intended to be used in `platform_init`/`platform_deinit`-like APIs.

`thread_notifications_dispatcher_get_call_dispatcher` can be safely called from multiple threads. 

## Exposed API

```c
TCALL_DISPATCHER_DEFINE_CALL_TYPE(THREAD_NOTIFICATION_CALL, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notification_reason);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(THREAD_NOTIFICATION_CALL));
TCALL_DISPATCHER_TYPE_DECLARE(THREAD_NOTIFICATION_CALL, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notification_reason);

MOCKABLE_FUNCTION(, int, thread_notifications_dispatcher_init);
MOCKABLE_FUNCTION(, void, thread_notifications_dispatcher_deinit);

MOCKABLE_FUNCTION(, TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL), thread_notifications_dispatcher_get_call_dispatcher);
```

## Callbacks

```c
static void thread_notifications_lackey_dll_cb(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);
```

### thread_notifications_dispatcher_init

```c
MOCKABLE_FUNCTION(, int, thread_notifications_dispatcher_init);
```

`thread_notifications_dispatcher_init` initializes the module.

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_001: [** If the module is already initialized, `thread_notifications_dispatcher_init` shall fail and return a non-zero value. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_002: [** Otherwise, `thread_notifications_dispatcher_init` shall initialize the module: **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_003: [** `thread_notifications_dispatcher_init` shall create a `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_004: [** `thread_notifications_dispatcher_init` shall store in a global variable the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_005: [** `thread_notifications_dispatcher_init` shall call `thread_notifications_lackey_dll_init_callback` to register `thread_notifications_lackey_dll_cb` as the thread notifications callback. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_006: [** `thread_notifications_dispatcher_init` shall succeed and return 0. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_007: [** If any error occurrs, `thread_notifications_dispatcher_init` shall fail and return a non-zero value. **]**

### thread_notifications_dispatcher_init

```c
MOCKABLE_FUNCTION(, void, thread_notifications_dispatcher_deinit);
```

`thread_notifications_dispatcher_deinit` de-initializes the module.

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_008: [** If the module is not initialized, `thread_notifications_dispatcher_deinit` shall return. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_009: [** Otherwise, `thread_notifications_dispatcher_init` shall call `thread_notifications_lackey_dll_deinit_callback` to clear the thread notifications callback. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_010: [** `thread_notifications_dispatcher_deinit` shall release the reference to the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)`. **]**

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_011: [** `thread_notifications_dispatcher_deinit` shall de-initialize the module. **]**

### thread_notifications_dispatcher_get_call_dispatcher

```c
MOCKABLE_FUNCTION(, TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL), thread_notifications_dispatcher_get_call_dispatcher);
```

`thread_notifications_dispatcher_get_call_dispatcher` gets the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)` singleton.

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_012: [** `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)` shall return the `TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL)` created in `thread_notifications_dispatcher_init`. **]**

### thread_notifications_lackey_dll_cb

```c
static void thread_notifications_lackey_dll_cb(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);
```

`thread_notifications_lackey_dll_cb` is called by the thread notifications lackey whenever a thread attach/detach occurs.

**SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_013: [** `thread_notifications_lackey_dll_cb` shall dispatch the call by calling `TCALL_DISPATCHER_DISPATCH_CALL(THREAD_NOTIFICATION_CALL)` with `reason` as argument. **]**
