# `tcall_dispatcher` requirements

## Overview

`TCALL_DISPATCHER` is a module that implement dispatching calls to a number of target call receivers.

`TCALL_DISPATCHER` is thread safe (registering/unregistering call targets and dispatching calls are thread safe).

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

`TCALL_DISPATCHER_TYPE_DECLARE(T)` is a macro to be used in a header declaration. It introduces the source and target function types and several APIs (as MOCKABLE_FUNCTIONS):

### TCALL_DISPATCHER_CREATE(T)
```c
static TCALL_DISPATCHER(T) TCALL_DISPATCHER_CREATE(T)(void);
```

`TCALL_DISPATCHER_CREATE(T)` creates a new `TCALL_DISPATCHER(T)`.

`TCALL_DISPATCHER_CREATE(T)` shall call `THANDLE_MALLOC` to allocate the result.

`TCALL_DISPATCHER_CREATE(T)` shall call `srw_lock_ll_init` to initialize the lock used by the `TCALL_DISPATCHER` instance.

`TCALL_DISPATCHER_CREATE(T)` shall call `DList_InitializeListHead` to initialize the doubly linked list that holds the target call registrations.

`TCALL_DISPATCHER_CREATE(T)` shall succeed and return a non-`NULL` value.

If there are any failures then `TCALL_DISPATCHER_CREATE(T)` shall fail and return `NULL`.

### TCALL_DISPATCHER_REGISTER_TARGET(T)
```c
TCALL_DISPATCHER_TARGET_HANDLE(T) TCALL_DISPATCHER_REGISTER_TARGET(T)(TCALL_DISPATCHER(T) tcall_dispatcher, void* function_to_call, void* call_context)
```

`TCALL_DISPATCHER_REGISTER_TARGET(T)` registers a target function pointer and context.

If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`.

If `function_to_call` is `NULL` then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`.

`TCALL_DISPATCHER_REGISTER_TARGET(T)` shall allocate memory for a `TCALL_DISPATCHER_TARGET_HANDLE(T)` that holds `function_to_call` and `context`.

`TCALL_DISPATCHER_REGISTER_TARGET(T)` shall acquire exclusivly the lock.

`TCALL_DISPATCHER_REGISTER_TARGET(T)` shall add the new `TCALL_DISPATCHER_TARGET_HANDLE(T)` containing `function_to_call` and `context` in the doubly linked list.

`TCALL_DISPATCHER_REGISTER_TARGET(T)` shall release the lock.

`TCALL_DISPATCHER_REGISTER_TARGET(T)` shall succeed and return the `TCALL_DISPATCHER_TARGET_HANDLE(T)`.

If there are any failures then `TCALL_DISPATCHER_REGISTER_TARGET(T)` shall fail and return `NULL`.

### TCALL_DISPATCHER_UNREGISTER_TARGET(T)
```c
int TCALL_DISPATCHER_UNREGISTER_TARGET(T)(TCALL_DISPATCHER(T) tcall_dispatcher, TCALL_DISPATCHER_TARGET_HANDLE(T) call_target)
```

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` unregisters a call target.

If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall fail and return a non-zero value.

If `call_target` is `NULL` then `TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall fail and return a non-zero value.

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall acquire exclusivly the lock.

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall remove from the doubly linked list the call target `call_target`.

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall release the lock.

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall free the `call_target` resources.

`TCALL_DISPATCHER_UNREGISTER_TARGET(T)` shall succeed and return 0.

### TCALL_DISPATCHER_DISPATCH_CALL(T)
```c
int TCALL_DISPATCHER_DISPATCH_CALL(T)(TCALL_DISPATCHER(T) tcall_dispatcher, ...)
```

`TCALL_DISPATCHER_DISPATCH_CALL(T)` calls each registered call target with the parameters passed in `...`.

If `tcall_dispatcher` is `NULL` then `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall fail and return a non-zero value.

Otherwise, `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall acquire the lock in shared mode.

For each call target that was registered, `TCALL_DISPATCHER_DISPATCH_CALL(T)` shall call  the `function_to_call` with the associated context and the parameters in `...`.

`TCALL_DISPATCHER_DISPATCH_CALL(T)` shall release the lock.

`TCALL_DISPATCHER_DISPATCH_CALL(T)` shall succeed and return 0.
