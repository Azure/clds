// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef TQUEUE_LL_H
#define TQUEUE_LL_H

#ifdef __cplusplus
#include <cinttypes>
#else // __cplusplus
#include <stdbool.h>
#include <inttypes.h>
#endif // __cplusplus

#include "c_pal/interlocked.h"

#include "c_pal/thandle_ll.h"

#include "umock_c/umock_c_prod.h"

#define TQUEUE_PUSH_RESULT_VALUES \
    TQUEUE_PUSH_OK, \
    TQUEUE_PUSH_INVALID_ARG, \
    TQUEUE_PUSH_QUEUE_FULL \

MU_DEFINE_ENUM(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_RESULT_VALUES);

#define TQUEUE_POP_RESULT_VALUES \
    TQUEUE_POP_OK, \
    TQUEUE_POP_INVALID_ARG, \
    TQUEUE_POP_QUEUE_EMPTY, \
    TQUEUE_POP_REJECTED

MU_DEFINE_ENUM(TQUEUE_POP_RESULT, TQUEUE_POP_RESULT_VALUES);

#define QUEUE_ENTRY_STATE_VALUES \
    QUEUE_ENTRY_STATE_NOT_USED, \
    QUEUE_ENTRY_STATE_PUSHING, \
    QUEUE_ENTRY_STATE_USED, \
    QUEUE_ENTRY_STATE_POPPING \

MU_DEFINE_ENUM(QUEUE_ENTRY_STATE, QUEUE_ENTRY_STATE_VALUES);

/*TQUEUE is backed by a THANDLE build on the structure below*/
#define TQUEUE_STRUCT_TYPE_NAME_TAG(T) MU_C2(TQUEUE_TYPEDEF_NAME(T), _TAG)
#define TQUEUE_TYPEDEF_NAME(T) MU_C2(TQUEUE_STRUCT_, T)

#define TQUEUE_ENTRY_STRUCT_TYPE_NAME_TAG(T) MU_C2(TQUEUE_ENTRY_STRUCT_TYPE_NAME(T), _TAG)
#define TQUEUE_ENTRY_STRUCT_TYPE_NAME(T) MU_C2(TQUEUE_ENTRY_STRUCT_, T)

/* This introduces the name for the copy item function */
#define TQUEUE_DEFINE_COPY_ITEM_FUNCTION_TYPE_NAME(T) MU_C2(TQUEUE_COPY_ITEM_FUNC_TYPE_, T)
#define TQUEUE_COPY_ITEM_FUNC(T) TQUEUE_DEFINE_COPY_ITEM_FUNCTION_TYPE_NAME(T)

/* This introduces the name for the dispose item function */
#define TQUEUE_DEFINE_DISPOSE_ITEM_FUNCTION_TYPE_NAME(T) MU_C2(TQUEUE_DISPOSE_ITEM_FUNC_TYPE_, T)
#define TQUEUE_DISPOSE_ITEM_FUNC(T) TQUEUE_DEFINE_DISPOSE_ITEM_FUNCTION_TYPE_NAME(T)

/* This introduces the name for the pop condition function */
#define TQUEUE_DEFINE_CONDITION_FUNCTION_TYPE_NAME(T) MU_C2(TQUEUE_CONDITION_FUNC_TYPE_, T)
#define TQUEUE_CONDITION_FUNC(T) TQUEUE_DEFINE_CONDITION_FUNCTION_TYPE_NAME(T)

/*TQUEUE_DEFINE_STRUCT_TYPE(T) introduces the base type that holds the queue typed as T*/
#define TQUEUE_DEFINE_STRUCT_TYPE(T)                                                                            \
typedef void (*TQUEUE_DEFINE_COPY_ITEM_FUNCTION_TYPE_NAME(T))(void* context, T* dst, T* src);                   \
typedef void (*TQUEUE_DEFINE_DISPOSE_ITEM_FUNCTION_TYPE_NAME(T))(void* context, T* item);                       \
typedef bool (*TQUEUE_DEFINE_CONDITION_FUNCTION_TYPE_NAME(T))(void* context, T* item);                          \
typedef struct TQUEUE_ENTRY_STRUCT_TYPE_NAME_TAG(T)                                                             \
{                                                                                                               \
    union                                                                                                       \
    {                                                                                                           \
        volatile_atomic int32_t state;                                                                          \
        volatile_atomic QUEUE_ENTRY_STATE state_as_enum;                                                        \
    };                                                                                                          \
    T value;                                                                                                    \
} TQUEUE_ENTRY_STRUCT_TYPE_NAME(T);                                                                             \
typedef struct TQUEUE_STRUCT_TYPE_NAME_TAG(T)                                                                   \
{                                                                                                               \
    volatile_atomic int64_t head;                                                                               \
    volatile_atomic int64_t tail;                                                                               \
    TQUEUE_COPY_ITEM_FUNC(T) copy_item_function;                                                                \
    TQUEUE_DISPOSE_ITEM_FUNC(T) dispose_item_function;                                                          \
    void* dispose_item_function_context;                                                                        \
    uint32_t queue_size;                                                                                        \
    TQUEUE_ENTRY_STRUCT_TYPE_NAME(T) queue[];                                                                   \
} TQUEUE_TYPEDEF_NAME(T);                                                                                       \

/*TQUEUE is-a THANDLE*/
/*given a type "T" TQUEUE_LL(T) expands to the name of the type. */
#define TQUEUE_LL(T) THANDLE(TQUEUE_TYPEDEF_NAME(T))

/*because TQUEUE is a THANDLE, all THANDLE's macro APIs are useable with TQUEUE.*/
/*the below are just shortcuts of THANDLE's public ones*/
#define TQUEUE_LL_INITIALIZE(T) THANDLE_INITIALIZE(TQUEUE_TYPEDEF_NAME(T))
#define TQUEUE_LL_ASSIGN(T) THANDLE_ASSIGN(TQUEUE_TYPEDEF_NAME(T))
#define TQUEUE_LL_MOVE(T) THANDLE_MOVE(TQUEUE_TYPEDEF_NAME(T))
#define TQUEUE_LL_INITIALIZE_MOVE(T) THANDLE_INITIALIZE_MOVE(TQUEUE_TYPEDEF_NAME(T))

/*introduces a new name for a function that returns a TQUEUE_LL(T)*/
#define TQUEUE_LL_CREATE_NAME(C) MU_C2(TQUEUE_LL_CREATE_, C)
#define TQUEUE_LL_CREATE(C) TQUEUE_LL_CREATE_NAME(C)

/*introduces a new name for the push function */
#define TQUEUE_LL_PUSH_NAME(C) MU_C2(TQUEUE_LL_PUSH_, C)
#define TQUEUE_LL_PUSH(C) TQUEUE_LL_PUSH_NAME(C)

/*introduces a new name for the pop function */
#define TQUEUE_LL_POP_NAME(C) MU_C2(TQUEUE_LL_POP_, C)
#define TQUEUE_LL_POP(C) TQUEUE_LL_POP_NAME(C)

/*introduces a function declaration for tqueue_create*/
#define TQUEUE_LL_CREATE_DECLARE(C, T) MOCKABLE_FUNCTION(, TQUEUE_LL(T), TQUEUE_LL_CREATE(C), uint32_t, queue_size, TQUEUE_COPY_ITEM_FUNC(T), copy_item_function, TQUEUE_DISPOSE_ITEM_FUNC(T), dispose_item_function, void*, dispose_item_function_context);

/*introduces a function declaration for tqueue_push*/
#define TQUEUE_LL_PUSH_DECLARE(C, T) MOCKABLE_FUNCTION(, TQUEUE_PUSH_RESULT, TQUEUE_LL_PUSH(C), TQUEUE_LL(T), tqueue, T*, item, void*, copy_item_function_context);

/*introduces a function declaration for tqueue_pop*/
#define TQUEUE_LL_POP_DECLARE(C, T) MOCKABLE_FUNCTION(, TQUEUE_POP_RESULT, TQUEUE_LL_POP(C), TQUEUE_LL(T), tqueue, T*, item, void*, copy_item_function_context, TQUEUE_CONDITION_FUNC(T), condition_function, void*, condition_function_context);

/*introduces a name for the function that free's a TQUEUE when it's ref count got to 0*/
#define TQUEUE_LL_FREE_NAME(C) MU_C2(TQUEUE_LL_FREE_, C)

/*introduces a function definition for freeing the allocated resources for a TQUEUE*/
#define TQUEUE_LL_FREE_DEFINE(C, T) \
static void TQUEUE_LL_FREE_NAME(C)(TQUEUE_TYPEDEF_NAME(T)* tqueue)                                                                                                  \
{                                                                                                                                                                   \
    if (tqueue == NULL)                                                                                                                                             \
    {                                                                                                                                                               \
        LogError("invalid arguments " MU_TOSTRING(TQUEUE_TYPEDEF_NAME(T)) "* tqueue=%p",                                                                            \
            tqueue);                                                                                                                                                \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        if (tqueue->dispose_item_function == NULL)                                                                                                                  \
        {                                                                                                                                                           \
            /* Codes_SRS_TQUEUE_01_008: [ If dispose_item_function passed to TQUEUE_CREATE(T) is NULL, TQUEUE_DISPOSE_FUNC(T) shall return. ]*/                     \
        }                                                                                                                                                           \
        else                                                                                                                                                        \
        {                                                                                                                                                           \
            /* Codes_SRS_TQUEUE_01_009: [ Otherwise, TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue head by calling interlocked_add_64. ]*/                  \
            int64_t current_head = interlocked_add_64((volatile_atomic int64_t*)&tqueue->head, 0);                                                                  \
            /* Codes_SRS_TQUEUE_01_010: [ TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue tail by calling interlocked_add_64. ]*/                             \
            int64_t current_tail = interlocked_add_64((volatile_atomic int64_t*)&tqueue->tail, 0);                                                                  \
            for (int64_t pos = current_tail; pos < current_head; pos++)                                                                                             \
            {                                                                                                                                                       \
                uint32_t index = (uint32_t)(pos % tqueue->queue_size);                                                                                              \
                /* Codes_SRS_TQUEUE_01_011: [ For each item in the queue, dispose_item_function shall be called with dispose_function_context and a pointer to the array entry value (T*). ]*/ \
                tqueue->dispose_item_function(tqueue->dispose_item_function_context, &tqueue->queue[index].value);                                                  \
            }                                                                                                                                                       \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
}                                                                                                                                                                   \

/*introduces a function definition for tqueue_create*/
#define TQUEUE_LL_CREATE_DEFINE(C, T)                                                                                                                               \
TQUEUE_LL(T) TQUEUE_LL_CREATE(C)(uint32_t queue_size, TQUEUE_COPY_ITEM_FUNC(T) copy_item_function, TQUEUE_DISPOSE_ITEM_FUNC(T) dispose_item_function, void* dispose_item_function_context) \
{                                                                                                                                                                   \
    TQUEUE_TYPEDEF_NAME(T)* result;                                                                                                                                 \
    bool is_copy_item_function_NULL = (copy_item_function == NULL);                                                                                                 \
    bool is_dispose_item_function_NULL = (dispose_item_function == NULL);                                                                                           \
    if (                                                                                                                                                            \
        /* Codes_SRS_TQUEUE_01_001: [ If queue_size is 0, TQUEUE_CREATE(T) shall fail and return NULL. ]*/                                                          \
        (queue_size == 0) ||                                                                                                                                        \
        /* Codes_SRS_TQUEUE_01_002: [ If any of copy_item_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/ \
        ((is_copy_item_function_NULL || is_dispose_item_function_NULL) &&                                                                                           \
         !(is_copy_item_function_NULL && is_dispose_item_function_NULL))                                                                                            \
       )                                                                                                                                                            \
    {                                                                                                                                                               \
        LogError("Invalid arguments: uint32_t queue_size=%" PRIu32 ", " MU_TOSTRING(TQUEUE_COPY_ITEM_FUNC(T)) " copy_item_function=%p, " MU_TOSTRING(TQUEUE_DISPOSE_ITEM_FUNC(T)) " dispose_item_function=%p, void* dispose_item_function_context=%p", \
            queue_size, copy_item_function, dispose_item_function, dispose_item_function_context);                                                        \
        result = NULL;                                                                                                                                              \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TQUEUE_01_003: [ TQUEUE_CREATE(T) shall call THANDLE_MALLOC_FLEX with TQUEUE_DISPOSE_FUNC(T) as dispose function, nmemb set to queue_size and size set to sizeof(T). ]*/ \
        result = THANDLE_MALLOC_FLEX(TQUEUE_TYPEDEF_NAME(C))(TQUEUE_LL_FREE_NAME(C), queue_size, sizeof(TQUEUE_ENTRY_STRUCT_TYPE_NAME(T)));                         \
        if(result == NULL)                                                                                                                                          \
        {                                                                                                                                                           \
            /* Codes_SRS_TQUEUE_01_007: [ If there are any failures then TQUEUE_CREATE(T) shall fail and return NULL. ]*/                                           \
            LogError("failure in " MU_TOSTRING(THANDLE_MALLOC_FLEX) "(sizeof(" MU_TOSTRING(TQUEUE_TYPEDEF_NAME(C)) ")=%zu, queue_size=%" PRIu32 ", sizeof(" MU_TOSTRING(TQUEUE_ENTRY_STRUCT_TYPE_NAME(T)) ")=%zu)", \
                sizeof(TQUEUE_TYPEDEF_NAME(C)), queue_size, sizeof(TQUEUE_ENTRY_STRUCT_TYPE_NAME(T)));                                                              \
            /*return as is*/                                                                                                                                        \
        }                                                                                                                                                           \
        else                                                                                                                                                        \
        {                                                                                                                                                           \
            result->queue_size = queue_size;                                                                                                                        \
            result->copy_item_function = copy_item_function;                                                                                                        \
            result->dispose_item_function = dispose_item_function;                                                                                                  \
            result->dispose_item_function_context = dispose_item_function_context;                                                                                  \
            /* Codes_SRS_TQUEUE_01_004: [ TQUEUE_CREATE(T) shall initialize the head and tail of the list with 0 by using interlocked_exchange_64. ]*/              \
            (void)interlocked_exchange_64(&result->head, 0);                                                                                                        \
            (void)interlocked_exchange_64(&result->tail, 0);                                                                                                        \
            for (uint32_t i = 0; i < queue_size; i++)                                                                                                               \
            {                                                                                                                                                       \
                /* Codes_SRS_TQUEUE_01_005: [ TQUEUE_CREATE(T) shall initialize the state for each entry in the array used for the queue with NOT_USED by using interlocked_exchange. ]*/ \
                (void)interlocked_exchange(&result->queue[i].state, QUEUE_ENTRY_STATE_NOT_USED);                                                                    \
            }                                                                                                                                                       \
            /* Codes_SRS_TQUEUE_01_006: [ TQUEUE_CREATE(T) shall succeed and return a non-NULL value. ]*/                                                           \
            /*return as is*/                                                                                                                                        \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}

/*introduces a function definition for tqueue_push*/
#define TQUEUE_LL_PUSH_DEFINE(C, T)                                                                                                                                 \
TQUEUE_PUSH_RESULT TQUEUE_LL_PUSH(C)(TQUEUE_LL(T) tqueue, T* item, void* copy_item_function_context)                                                                \
{                                                                                                                                                                   \
    TQUEUE_PUSH_RESULT result;                                                                                                                                      \
    if (                                                                                                                                                            \
        /* Codes_SRS_TQUEUE_01_012: [ If tqueue is NULL then TQUEUE_PUSH(T) shall fail and return TQUEUE_PUSH_INVALID_ARG. ]*/                                      \
        (tqueue == NULL) ||                                                                                                                                         \
        /* Codes_SRS_TQUEUE_01_013: [ If item is NULL then TQUEUE_PUSH(T) shall fail and return TQUEUE_PUSH_INVALID_ARG. ]*/                                        \
        (item == NULL)                                                                                                                                              \
       )                                                                                                                                                            \
    {                                                                                                                                                               \
        LogError("Invalid arguments: TQUEUE_LL(" MU_TOSTRING(T) ") tqueue=%p, const " MU_TOSTRING(T) "* item=%p, void* copy_item_function_context=%p",              \
            tqueue, item, copy_item_function_context);                                                                                                              \
        result = TQUEUE_PUSH_INVALID_ARG;                                                                                                                           \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TQUEUE_01_014: [ TQUEUE_PUSH(T) shall execute the following actions until it is either able to push the item in the queue or the queue is full: ]*/ \
        do                                                                                                                                                          \
        {                                                                                                                                                           \
            /* Codes_SRS_TQUEUE_01_015: [ TQUEUE_PUSH(T) shall obtain the current head queue by calling interlocked_add_64. ]*/                                     \
            int64_t current_head = interlocked_add_64((volatile_atomic int64_t*)&tqueue->head, 0);                                                                  \
            /* Codes_SRS_TQUEUE_01_016: [ TQUEUE_PUSH(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/                                     \
            int64_t current_tail = interlocked_add_64((volatile_atomic int64_t*)&tqueue->tail, 0);                                                                  \
            if (current_head >= current_tail + tqueue->queue_size)                                                                                                  \
            {                                                                                                                                                       \
                /* Codes_SRS_TQUEUE_01_022: [ If the queue is full (current head >= current tail + queue size), TQUEUE_PUSH(T) shall return TQUEUE_PUSH_QUEUE_FULL. ]*/ \
                result = TQUEUE_PUSH_QUEUE_FULL;                                                                                                                    \
                break;                                                                                                                                              \
            }                                                                                                                                                       \
            else                                                                                                                                                    \
            {                                                                                                                                                       \
                uint32_t index = (uint32_t)(current_head % tqueue->queue_size);                                                                                     \
                /* Codes_SRS_TQUEUE_01_017: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall change the head array entry state to PUSHING (from NOT_USED). ]*/ \
                if (interlocked_compare_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED) != QUEUE_ENTRY_STATE_NOT_USED) \
                {                                                                                                                                                   \
                    /* Codes_SRS_TQUEUE_01_023: [ If the state of the array entry corresponding to the head is not NOT_USED, TQUEUE_PUSH(T) shall retry the whole push. ]*/ \
                    /* likely queue full */                                                                                                                         \
                    continue;                                                                                                                                       \
                }                                                                                                                                                   \
                else                                                                                                                                                \
                {                                                                                                                                                   \
                    /* Codes_SRS_TQUEUE_01_018: [ Using interlocked_compare_exchange_64, TQUEUE_PUSH(T) shall replace the head value with the head value obtained earlier + 1. ]*/ \
                    if (interlocked_compare_exchange_64((volatile_atomic int64_t*)&tqueue->head, current_head + 1, current_head) != current_head)                   \
                    {                                                                                                                                               \
                        /* Codes_SRS_TQUEUE_01_043: [ If the queue head has changed, TQUEUE_PUSH(T) shall set the state back to NOT_USED and retry the push. ]*/    \
                        (void)interlocked_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_NOT_USED);                              \
                        continue;                                                                                                                                   \
                    }                                                                                                                                               \
                    else                                                                                                                                            \
                    {                                                                                                                                               \
                        if (tqueue->copy_item_function == NULL)                                                                                                     \
                        {                                                                                                                                           \
                            /* Codes_SRS_TQUEUE_01_019: [ If no copy_item_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall copy the value of item into the array entry value whose state was changed to PUSHING. ]*/ \
                            (void)memcpy((void*)&tqueue->queue[index].value, (void*)item, sizeof(T));                                                               \
                        }                                                                                                                                           \
                        else                                                                                                                                        \
                        {                                                                                                                                           \
                            /* Codes_SRS_TQUEUE_01_024: [ If a copy_item_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall call the copy_item_function with copy_item_function_context as context, a pointer to the array entry value whose state was changed to PUSHING as push_dst and item as push_src. ] */ \
                            tqueue->copy_item_function(copy_item_function_context, (T*)&tqueue->queue[index].value, item);                                          \
                        }                                                                                                                                           \
                        /* Codes_SRS_TQUEUE_01_020: [ TQUEUE_PUSH(T) shall set the state to USED by using interlocked_exchange. ]*/                                 \
                        (void)interlocked_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_USED);                                  \
                        /* Codes_SRS_TQUEUE_01_021: [ TQUEUE_PUSH(T) shall succeed and return TQUEUE_PUSH_OK. ]*/                                                   \
                        result = TQUEUE_PUSH_OK;                                                                                                                    \
                        break;                                                                                                                                      \
                    }                                                                                                                                               \
                }                                                                                                                                                   \
            }                                                                                                                                                       \
        } while (1);                                                                                                                                                \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}                                                                                                                                                                   \

/*introduces a function definition for tqueue_pop*/
#define TQUEUE_LL_POP_DEFINE(C, T)                                                                                                                                  \
TQUEUE_POP_RESULT TQUEUE_LL_POP(C)(TQUEUE_LL(T) tqueue, T* item, void* copy_item_function_context, TQUEUE_CONDITION_FUNC(T) condition_function, void* condition_function_context) \
{                                                                                                                                                                   \
    TQUEUE_POP_RESULT result;                                                                                                                                       \
    if (                                                                                                                                                            \
        /* Codes_SRS_TQUEUE_01_025: [ If tqueue is NULL then TQUEUE_POP(T) shall fail and return TQUEUE_POP_INVALID_ARG. ]*/                                        \
        (tqueue == NULL) ||                                                                                                                                         \
        /* Codes_SRS_TQUEUE_01_027: [ If item is NULL then TQUEUE_POP(T) shall fail and return TQUEUE_POP_INVALID_ARG. ]*/                                          \
        (item == NULL)                                                                                                                                              \
       )                                                                                                                                                            \
    {                                                                                                                                                               \
        LogError("Invalid arguments: TQUEUE_LL(" MU_TOSTRING(T) ") tqueue=%p, " MU_TOSTRING(T) "*=%p, void* copy_item_function_context, TQUEUE_CONDITION_FUNC(T) condition_function, void* condition_function_context", \
            tqueue, item);                                                                                                                                          \
        result = TQUEUE_POP_INVALID_ARG;                                                                                                                            \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TQUEUE_01_026: [ TQUEUE_POP(T) shall execute the following actions until it is either able to pop the item from the queue or the queue is empty: ] */ \
        do                                                                                                                                                          \
        {                                                                                                                                                           \
            /* Codes_SRS_TQUEUE_01_028: [ TQUEUE_POP(T) shall obtain the current head queue by calling interlocked_add_64. ]*/                                      \
            int64_t current_head = interlocked_add_64((volatile_atomic int64_t*)&tqueue->head, 0);                                                                  \
            /* Codes_SRS_TQUEUE_01_029: [ TQUEUE_POP(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/                                      \
            int64_t current_tail = interlocked_add_64((volatile_atomic int64_t*)&tqueue->tail, 0);                                                                  \
            if (current_tail >= current_head)                                                                                                                       \
            {                                                                                                                                                       \
                /* Codes_SRS_TQUEUE_01_035: [ If the queue is empty (current tail >= current head), TQUEUE_POP(T) shall return TQUEUE_POP_QUEUE_EMPTY. ]*/          \
                result = TQUEUE_POP_QUEUE_EMPTY;                                                                                                                    \
                break;                                                                                                                                              \
            }                                                                                                                                                       \
            else                                                                                                                                                    \
            {                                                                                                                                                       \
                /* LogInfo("Popping from %" PRId64 "", current_tail); */ \
                /* Codes_SRS_TQUEUE_01_030: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall set the tail array entry state to POPPING (from USED). ]*/   \
                uint32_t index = (uint32_t)(current_tail % tqueue->queue_size);                                                                                     \
                if (interlocked_compare_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED) != QUEUE_ENTRY_STATE_USED) \
                {                                                                                                                                                   \
                    /* Codes_SRS_TQUEUE_01_036: [ If the state of the array entry corresponding to the tail is not USED, TQUEUE_POP(T) shall try again. ]*/         \
                    continue;                                                                                                                                       \
                }                                                                                                                                                   \
                else                                                                                                                                                \
                {                                                                                                                                                   \
                    bool should_pop;                                                                                                                                \
                    /* Codes_SRS_TQUEUE_01_039: [ If condition_function is not NULL: ]*/                                                                            \
                    if (condition_function != NULL)                                                                                                                 \
                    {                                                                                                                                               \
                        /* Codes_SRS_TQUEUE_01_040: [ TQUEUE_POP(T) shall call condition_function with condition_function_context and a pointer to the array entry value whose state was changed to POPPING. ] */ \
                        should_pop = condition_function(condition_function_context, (T*)&tqueue->queue[index].value);                                               \
                    }                                                                                                                                               \
                    else                                                                                                                                            \
                    {                                                                                                                                               \
                        /* Codes_SRS_TQUEUE_01_042: [ Otherwise, shall proceed with the pop. ]*/                                                                    \
                        should_pop = true;                                                                                                                          \
                    }                                                                                                                                               \
                    if (!should_pop)                                                                                                                                \
                    {                                                                                                                                               \
                        /* Codes_SRS_TQUEUE_01_041: [ If condition_function returns false, TQUEUE_POP(T) shall set the state to USED by using interlocked_exchange and return TQUEUE_POP_REJECTED. ]*/ \
                        (void)interlocked_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_USED);                                  \
                        result = TQUEUE_POP_REJECTED;                                                                                                               \
                    }                                                                                                                                               \
                    else                                                                                                                                            \
                    {                                                                                                                                               \
                        /* Codes_SRS_TQUEUE_01_031: [ TQUEUE_POP(T) shall replace the tail value with the tail value obtained earlier + 1 by using interlocked_exchange_64. ]*/ \
                        if (interlocked_compare_exchange_64((volatile_atomic int64_t*)&tqueue->tail, current_tail + 1, current_tail) != current_tail)               \
                        {                                                                                                                                           \
                            /* Codes_SRS_TQUEUE_01_044: [ If incrementing the tail by using interlocked_compare_exchange_64 does not succeed, TQUEUE_POP(T) shall revert the state of the array entry to USED and retry. ]*/ \
                            (void)interlocked_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_USED);                              \
                            continue;                                                                                                                               \
                        }                                                                                                                                           \
                        else                                                                                                                                        \
                        {                                                                                                                                           \
                            if (tqueue->copy_item_function == NULL)                                                                                                 \
                            {                                                                                                                                       \
                                /* Codes_SRS_TQUEUE_01_032: [ If a copy_item_function was not specified in TQUEUE_CREATE(T): ]*/                                    \
                                /* Codes_SRS_TQUEUE_01_033: [ TQUEUE_POP(T) shall copy array entry value whose state was changed to POPPING to item. ]*/            \
                                (void)memcpy((void*)item, (void*)&tqueue->queue[index].value, sizeof(T));                                                       \
                            }                                                                                                                                       \
                            else                                                                                                                                    \
                            {                                                                                                                                       \
                                /* Codes_SRS_TQUEUE_01_037: [ If copy_item_function and sispose_item_function were specified in TQUEUE_CREATE(T): ]*/               \
                                /* Codes_SRS_TQUEUE_01_038: [ TQUEUE_POP(T) shall call copy_item_function with copy_item_function_context as context, the array entry value whose state was changed to POPPING to item as pop_src and item as pop_dst. ]*/ \
                                tqueue->copy_item_function(copy_item_function_context, item, (T*)&tqueue->queue[index].value);                                      \
                            }                                                                                                                                       \
                            if (tqueue->dispose_item_function != NULL)                                                                                              \
                            {                                                                                                                                       \
                                /* Codes_SRS_TQUEUE_01_045: [ TQUEUE_POP(T) shall call dispose_item_function with dispose_item_function_context as context and the array entry value whose state was changed to POPPING as item. ]*/ \
                                tqueue->dispose_item_function(tqueue->dispose_item_function_context, (T*)&tqueue->queue[index].value);                              \
                            }                                                                                                                                       \
                            /* Codes_SRS_TQUEUE_01_034: [ TQUEUE_POP(T) shall set the state to NOT_USED by using interlocked_exchange, succeed and return TQUEUE_POP_OK. ]*/ \
                            (void)interlocked_exchange((volatile_atomic int32_t*)&tqueue->queue[index].state, QUEUE_ENTRY_STATE_NOT_USED);                          \
                            result = TQUEUE_POP_OK;                                                                                                                 \
                        }                                                                                                                                           \
                    }                                                                                                                                               \
                    break;                                                                                                                                          \
                }                                                                                                                                                   \
            }                                                                                                                                                       \
        } while (1);                                                                                                                                                \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}                                                                                                                                                                   \

/*macro to be used in headers*/                                                                                                     \
#define TQUEUE_LL_TYPE_DECLARE(C, T, ...)                                                                                           \
    /*hint: have TQUEUE_DEFINE_STRUCT_TYPE(T) before TQUEUE_LL_TYPE_DECLARE*/                                                       \
    THANDLE_LL_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(C), TQUEUE_TYPEDEF_NAME(T))                                                         \
    TQUEUE_LL_CREATE_DECLARE(C, T)                                                                                                  \
    TQUEUE_LL_PUSH_DECLARE(C, T)                                                                                                    \
    TQUEUE_LL_POP_DECLARE(C, T)                                                                                                     \

/*macro to be used in .c*/                                                                                                          \
#define TQUEUE_LL_TYPE_DEFINE(C, T, ...)                                                                                            \
    /*hint: have THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(T)) before TQUEUE_LL_TYPE_DEFINE*/                                         \
    TQUEUE_LL_FREE_DEFINE(C, T)                                                                                                     \
    TQUEUE_LL_CREATE_DEFINE(C, T)                                                                                                   \
    TQUEUE_LL_PUSH_DEFINE(C, T)                                                                                                     \
    TQUEUE_LL_POP_DEFINE(C, T)                                                                                                      \

#endif  /*TQUEUE_LL_H*/
