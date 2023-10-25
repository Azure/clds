# `tcall_dispatcher` requirements

## Overview

`TCALL_DISPATCHER` is a module that implement dispatching calls to a number of target call receivers.

`TCALL_DISPATCHER` is thread safe (registering/unregistering call targets and dispatching calls are thread safe).

At the moment re-entrancy is not supported (the user should not call register/unregister from the call targets).
Further improvements to the module will remove this limitation.

The module provides the following functionality:

- Registering a target (with an associated context) to be called when the dispatch call is executed.
- Unregistering a target.
- Calling all registered targets when the dispatch is called.

`TCALL_DISPATCHER` provides macros by which the user can specify the source call function signature.

The source call always returns void.

The target call signature looks similar to the source call signature, but it prepends in the list of parameters the context passed by the user when registering the target.

Because `TCALL_DISPATCHER` is a kind of `THANDLE`, all of `THANDLE`'s APIs apply to `TCALL_DISPATCHER`. For convenience the following macros are provided out of the box with the same semantics as those of `THANDLE`'s:

`TCALL_DISPATCHER_INITIALIZE(T)`
`TCALL_DISPATCHER_ASSIGN(T)`
`TCALL_DISPATCHER_MOVE(T)`
`TCALL_DISPATCHER_INITIALIZE_MOVE(T)`

## Design

`TCALL_DISPATCHER` uses an `SRW_LOCK` and a `doubly_linked_list` to store the call targets.

The lock shall be acquired in exclusive mode whenever a register/unregister action is performed.

The lock shall be acquired in shared mode when the list has to be walked in order to call all the targets.

## Exposed API

```c

/*to be used as the type of handle*/
#define TCALL_DISPATCHER(T)

/*to be used in a header file*/
#define TCALL_DISPATCHER_TYPE_DECLARE(T)

/*to be used in a .c file*/
#define TCALL_DISPATCHER_TYPE_DEFINE(T)

```

### TCALL_DISPATCHER(T)

```c
#define TCALL_DISPATCHER(T) 
```
`TCALL_DISPATCHER(T)` is a `THANDLE`(`TCALL_DISPATCHER_STRUCT_T`), where `TCALL_DISPATCHER_STRUCT_T` is a structure introduced like below:
```c
typedef struct TCALL_DISPATCHER_STRUCT_T_TAG
{
    ...
} TCALL_DISPATCHER_STRUCT_T;
```

### TCALL_DISPATCHER_TYPE_DECLARE(T, ...)
```c
#define TCALL_DISPATCHER_TYPE_DECLARE(T, ...)
```

`TCALL_DISPATCHER_TYPE_DECLARE(T)` is a macro to be used in a header declaration.

It introduces the APIs (as MOCKABLE_FUNCTIONS) that can be called for a `TCALL_DISPATCHER`.

It introduces also the target call function type `TCALL_DISPATCHER_TARGET_FUNC(T)` based on the arguments in `...`.

It also introduces the type `TCALL_DISPATCHER_TARGET_HANDLE(T)`, which represents a handle to a call target.

`...` is a list of argtype argname pairs, example usage being:

```c
TCALL_DISPATCHER_TYPE_DECLARE(FOO, int, x, char*, y);
```

### TCALL_DISPATCHER_TYPE_DEFINE(T, ...)
```c
#define TCALL_DISPATCHER_TYPE_DEFINE(T, ...)
```

`TCALL_DISPATCHER_TYPE_DEFINE(T)` is a macro to be used in a .c file to define all the needed functions for `TCALL_DISPATCHER(T)`.

`...` is a list of argtype argname pairs, example usage being:

```c
TCALL_DISPATCHER_TYPE_DEFINE(FOO, int, x, char*, y);
```

### TCALL_DISPATCHER_CREATE(T)
```c
static TCALL_DISPATCHER(T) TCALL_DISPATCHER_CREATE(T)(void);
```

`TCALL_DISPATCHER_CREATE(T)` creates a new `TCALL_DISPATCHER(T)`.

**SRS_TCALL_DISPATCHER_01_001: [** `TCALL_DISPATCHER_CREATE(T)` shall call `THANDLE_MALLOC` with `TCALL_DISPATCHER_LL_FREE_{T}` as dispose function. **]**

**SRS_TCALL_DISPATCHER_01_002: [** `TCALL_DISPATCHER_CREATE(T)` shall call `srw_lock_ll_init` to initialize the lock used by the `TCALL_DISPATCHER` instance. **]**

**SRS_TCALL_DISPATCHER_01_003: [** `TCALL_DISPATCHER_CREATE(T)` shall call `DList_InitializeListHead` to initialize the doubly linked list that holds the target call registrations. **]**

**SRS_TCALL_DISPATCHER_01_004: [** `TCALL_DISPATCHER_CREATE(T)` shall succeed and return a non-`NULL` value. **]**

**SRS_TCALL_DISPATCHER_01_005: [** If there are any failures then `TCALL_DISPATCHER_CREATE(T)` shall fail and return `NULL`. **]**

### TCALL_DISPATCHER_LL_FREE_{T}

```c
static void TCALL_DISPATCHER_LL_FREE_{T}(TCALL_DISPATCHER_TYPEDEF_NAME(T)* tcall_dispatcher)
```

`TCALL_DISPATCHER_LL_FREE_{T}` disposes of the resources used by the `TCALL_DISPATCHER(T)`.

**SRS_TCALL_DISPATCHER_01_026: [** `TCALL_DISPATCHER_LL_FREE_{T}` shall call `srw_lock_ll_deinit` to de-initialize the lock. **]**

**SRS_TCALL_DISPATCHER_01_027: [** For each call target in the list, `TCALL_DISPATCHER_LL_FREE_{T}` shall free the resources associated with the call target. **]**

### TCALL_DISPATCHER_REGISTER_TARGET(T)
```c
TCALL_DISPATCHER_TARGET_HANDLE(T) TCALL_DISPATCHER_REGISTER_TARGET(T)(TCALL_DISPATCHER(T) tcall_dispatcher, TCALL_DISPATCHER_TARGET_FUNC(T) function_to_call, void* call_context)
```

`TCALL_DISPATCHER_REGISTER_TARGET(T)` registers a target function pointer and context.

**SRS_TCALL_DISPATCHER_01_006: [** If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`. **]**

**SRS_TCALL_DISPATCHER_01_007: [** If `function_to_call` is `NULL` then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`. **]**

**SRS_TCALL_DISPATCHER_01_008: [** `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall allocate memory for a `TCALL_DISPATCHER_TARGET_HANDLE(T)` that holds `function_to_call` and `context`. **]**

**SRS_TCALL_DISPATCHER_01_009: [** `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall acquire exclusivly the lock. **]**

**SRS_TCALL_DISPATCHER_01_010: [** `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall add the new `TCALL_DISPATCHER_TARGET_HANDLE(T)` containing `function_to_call` and `context` in the doubly linked list. **]**

**SRS_TCALL_DISPATCHER_01_011: [** `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall release the lock. **]**

**SRS_TCALL_DISPATCHER_01_012: [** `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall succeed and return the `TCALL_DISPATCHER_TARGET_HANDLE(T)`. **]**

**SRS_TCALL_DISPATCHER_01_013: [** If there are any failures then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`. **]**

### TCALL_DISPATCHER_UNREGISTER_TARGET(T)
```c
int TCALL_DISPATCHER_UNREGISTER_TARGET(T)(TCALL_DISPATCHER(T) tcall_dispatcher, TCALL_DISPATCHER_TARGET_HANDLE(T) call_target)
```

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` unregisters a call target.

**SRS_TCALL_DISPATCHER_01_014: [** If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall fail and return a non-zero value. **]**

**SRS_TCALL_DISPATCHER_01_015: [** If `call_target` is `NULL` then `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall fail and return a non-zero value. **]**

**SRS_TCALL_DISPATCHER_01_016: [** `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall acquire exclusivly the lock. **]**

**SRS_TCALL_DISPATCHER_01_017: [** `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall remove from the doubly linked list the call target `call_target`. **]**

**SRS_TCALL_DISPATCHER_01_018: [** `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall release the lock. **]**

**SRS_TCALL_DISPATCHER_01_019: [** `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall free the `call_target` resources. **]**

**SRS_TCALL_DISPATCHER_01_020: [** `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall succeed and return 0. **]**

### TCALL_DISPATCHER_DISPATCH_CALL(T)
```c
int TCALL_DISPATCHER_DISPATCH_CALL(T)(TCALL_DISPATCHER(T) tcall_dispatcher, ...)
```

`TCALL_DISPATCHER_DISPATCH_CALL(T)` calls each registered call target with the parameters passed in `...`.

**SRS_TCALL_DISPATCHER_01_021: [** If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall fail and return a non-zero value. **]**

**SRS_TCALL_DISPATCHER_01_022: [** Otherwise, `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall acquire the lock in shared mode. **]**

**SRS_TCALL_DISPATCHER_01_023: [** For each call target that was registered, `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall call  the `function_to_call` with the associated context and the parameters in `...`. **]**

**SRS_TCALL_DISPATCHER_01_024: [** `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall release the lock. **]**

**SRS_TCALL_DISPATCHER_01_025: [** `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall succeed and return 0. **]**
