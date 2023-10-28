# `tqueue` requirements

## Overview

`TQUEUE` is a module that implement a typed queue that holds elements of a certain type. `TQUEUE` is homogenous, all elements are of the same type.

`TQUEUE` is thread safe (push/pop calls are thread safe).

The module provides the following functionality:

- Create a queue with a given size.
- Push an element at the head of the queue.
- Pop an element from the tail of the queue. Pop returns the oldest element in the queue.
- Pop an element if it matches a given condition (a user condition function and a context are supplied as arguments to the call)

Because `TQUEUE` is a kind of `THANDLE`, all of `THANDLE`'s APIs apply to `TQUEUE`. For convenience the following macros are provided out of the box with the same semantics as those of `THANDLE`'s:

`TQUEUE_INITIALIZE(T)`
`TQUEUE_ASSIGN(T)`
`TQUEUE_MOVE(T)`
`TQUEUE_INITIALIZE_MOVE(T)`

## Design

`TQUEUE` uses an array of type `T` to store the elements in the queue.

The head and tail of the queue are `int64_t` values.

When pushing, the head value modulo queue size is used as array index where the element is written.

When popping, the tail value modulo queue size is used as array index where the element is read from.

A state is associated with each array entry.

The possible states for an array entry are:

- `NOT_USED` - No data exists at the entry.
- `PUSHING` - Data is currently being written at the entry due to a push. Data is not safe to be popped yet.
- `USED` - Data exists in the enty can can be popped.
- `POPPING` - Data is being popped and should not be popped by any other thread.

Note: plans exist to extend this queue into a growable queue.

## Exposed API

```c

/*to be used as the type of handle*/
#define TQUEUE(T)

/*to be used in a header file*/
#define TQUEUE_TYPE_DECLARE(T)

/*to be used in a .c file*/
#define TQUEUE_TYPE_DEFINE(T)

```

### TQUEUE(T)

```c
#define TQUEUE(T) 
```
`TQUEUE(T)` is a `THANDLE`(`TQUEUE_STRUCT_T`), where `TQUEUE_STRUCT_T` is a structure introduced like below:
```c
typedef struct TQUEUE_STRUCT_T_TAG
{
    ...
} TQUEUE_STRUCT_T;
```

### TQUEUE_TYPE_DECLARE(T)
```c
#define TQUEUE_TYPE_DECLARE(T)
```

`TQUEUE_TYPE_DECLARE(T)` is a macro to be used in a header declaration.

It introduces the APIs (as MOCKABLE_FUNCTIONS) that can be called for a `TQUEUE`.


```c
TQUEUE_TYPE_DECLARE(int32_t);
```

### TQUEUE_TYPE_DEFINE(T)
```c
#define TQUEUE_TYPE_DEFINE(T)
```

`TQUEUE_TYPE_DEFINE(T)` is a macro to be used in a .c file to define all the needed functions for `TQUEUE(T)`.

```c
TQUEUE_TYPE_DEFINE(int32_t);
```

### TQUEUE_CREATE(T)
```c
static TQUEUE(T) TQUEUE_CREATE(T)(uint32_t queue_size);
```

`TQUEUE_CREATE(T)` creates a new `TQUEUE(T)`.

`TQUEUE_CREATE(T)` shall call `THANDLE_MALLOC_WITH_EXTRA_SIZE` with `NULL` as dispose function and the extra size set to `queue_size` * sizeof(T).

`TQUEUE_CREATE(T)` shall initialize the head and tail of the list with 0.

`TQUEUE_CREATE(T)` shall initialize the state for each entry in the array used for the queue with `NOT_USED`.

`TQUEUE_CREATE(T)` shall succeed and return a non-`NULL` value.

If there are any failures then `TQUEUE_CREATE(T)` shall fail and return `NULL`.

### TQUEUE_PUSH(T)
```c
int TQUEUE_PUSH(T)(TQUEUE(T) tqueue, const T item)
```

`TQUEUE_PUSH(T)` pushes an item in the queue.

If `tqueue` is `NULL` then `TQUEUE_PUSH(T)` shall fail and return a non-zero value.

`TQUEUE_PUSH(T)` shall execute the following actions until it is either able to push the item in the queue or it fails:

- `TQUEUE_PUSH(T)` shall obtain the current head and tail of the queue.

- If the queue is full, `TQUEUE_PUSH(T)` shall fail and return a non-zero value.

- If the state of the array entry corresponding to the head is not `NOT_USED`, `TQUEUE_PUSH(T)` shall try again.

- If the state of the array entry corresponding to the head is `NOT_USED`:

  - `TQUEUE_PUSH(T)` shall set the state to `PUSHING`.

  - `TQUEUE_PUSH(T)` shall increment the head value.

  - `TQUEUE_PUSH(T)` shall copy the value of `item` into the array entry value that whose state was changed to `PUSHING`.

  - `TQUEUE_PUSH(T)` shall set the state to `USED`.

  - `TQUEUE_PUSH(T)` shall succeed and return 0.

### TQUEUE_POP(T)
```c
TQUEUE_POP_RESULT TQUEUE_POP(T)(TQUEUE(T) tqueue, T* item)
```

`TQUEUE_POP(T)` pops an item from the queue if available.

If `tqueue` is `NULL` then `TQUEUE_POP(T)` shall fail and return `TQUEUE_POP_ERROR`.

`TQUEUE_POP(T)` shall execute the following actions until it is either able to pop the item from the queue or it fails:

- `TQUEUE_POP(T)` shall obtain the current head and tail of the queue.

- If the queue is empty, `TQUEUE_POP(T)` shall fail and return `TQUEUE_POP_QUEUE_EMPTY`.

- If the state of the array entry corresponding to the tail is not `USED`, `TQUEUE_POP(T)` shall try again.

- If the state of the array entry corresponding to the tail is `USED`:

  - `TQUEUE_POP(T)` shall set the state to `POPPING`.

  - `TQUEUE_POP(T)` shall increment the tail value.

  - `TQUEUE_POP(T)` shall copy array entry value that whose state was changed to `POPPING` to `item`.

  - `TQUEUE_POP(T)` shall set the state to `NOT_USED`.

  - `TQUEUE_POP(T)` shall succeed and return `TQUEUE_POP_OK`.

### TQUEUE_POP_IF(T)
```c
TQUEUE_POP_IF_RESULT TQUEUE_POP_IF(T)(TQUEUE(T) tqueue, T* item, TQUEUE_DEFINE_CONDITION_FUNCTION_TYPE_NAME(T), condition_function, void*, context)
```

`TQUEUE_POP_IF(T)` pops an item from the queue conditionally.

The signature of the condition function is:

```c
bool TQUEUE_DEFINE_CONDITION_FUNCTION_TYPE_NAME(T)(void* context, const T* item);
```

If `tqueue` is `NULL` then `TQUEUE_POP_IF(T)` shall fail and return `TQUEUE_POP_IF_ERROR`.

`TQUEUE_POP_IF(T)` shall execute the following actions until it is either able to pop the item from the queue or it fails:

- `TQUEUE_POP_IF(T)` shall obtain the current head and tail of the queue.

- If the queue is empty, `TQUEUE_POP_IF(T)` shall fail and return `TQUEUE_POP_IF_QUEUE_EMPTY`.

- If the state of the array entry corresponding to the tail is not `USED`, `TQUEUE_POP_IF(T)` shall try again.

- If the state of the array entry corresponding to the tail is `USED`:

  - `TQUEUE_POP_IF(T)` shall set the state to `POPPING`.

  - `TQUEUE_POP_IF(T)` shall call `condition_function`, passing to it `context` and the address of the `T` value in the array entry corresponding to the tail of the queue.

  - If `condition_function` returns `false`, `TQUEUE_POP_IF(T)` shall change the state back to `USED` and return `TQUEUE_POP_IF_DOES_NOT_MATCH`.

  - Otherwise, `TQUEUE_POP_IF(T)` shall increment the tail value.

  - `TQUEUE_POP_IF(T)` shall copy array entry value that whose state was changed to `POPPING` to `item`.

  - `TQUEUE_POP_IF(T)` shall set the state to `NOT_USED`.

  - `TQUEUE_POP_IF(T)` shall succeed and return `TQUEUE_POP_IF_OK`.
