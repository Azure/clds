// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef TCALL_DISPATCHER_LL_H
#define TCALL_DISPATCHER_LL_H

#include "c_pal/interlocked.h"

#include "c_pal/thandle_ll.h"
#include "c_pal/srw_lock_ll.h"

#include "c_util/doublylinkedlist.h"

#include "umock_c/umock_c_prod.h"

/*TCALL_DISPATCHER is backed by a THANDLE build on the structure below*/
#define TCALL_DISPATCHER_STRUCT_TYPE_NAME_TAG(T) MU_C2(TCALL_DISPATCHER_TYPEDEF_NAME(T), _TAG)
#define TCALL_DISPATCHER_TYPEDEF_NAME(T) MU_C2(TCALL_DISPATCHER_STRUCT_, T)

#define TCALL_DISPATCHER_TARGET_STRUCT_TYPE_NAME_TAG(T) MU_C2(TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T), _TAG)
#define TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T) MU_C2(TCALL_DISPATCHER_TARGET_STRUCT_, T)
#define TCALL_DISPATCHER_TARGET_HANDLE_NAME(T) MU_C2(TCALL_DISPATCHER_TARGET_HANDLE_, T)

#define TCALL_DISPATCHER_TARGET_FUNC_TYPE_NAME(T) MU_C2(TCALL_DISPATCHER_TARGET_FUNC_, T)

// These macros help expand the arguments in the function signature
// This macro pastes the arguments as expected by a regular C function signature
#define TCALL_DISPATCHER_ARG_IN_SIGNATURE(arg_type, arg_name) , arg_type arg_name
// This macro pastes the arguments as expected by a MOCKABLE_FUNCTION signature (with commas between argument type and argument name)
#define TCALL_DISPATCHER_ARG_IN_MOCKABLE_SIGNATURE(arg_type, arg_name) , arg_type, arg_name
// This macro pastes only the argument name so that it is used when calling functions
#define TCALL_DISPATCHER_ARG_VALUE_IN_CALL(arg_type, arg_name) , arg_name

/*TCALL_DISPATCHER_DEFINE_CALL_TYPE(T) introduces the base type that holds the dispatcher typed as T*/
#define TCALL_DISPATCHER_DEFINE_CALL_TYPE(T, ...)                                                                     \
/*forward define the typedef of the TCALL_DISPATCHER struct so that it can be used for a function pointer definition*/  \
typedef void (*TCALL_DISPATCHER_TARGET_FUNC_TYPE_NAME(T))(void* context MU_FOR_EACH_2(TCALL_DISPATCHER_ARG_IN_SIGNATURE, __VA_ARGS__)); \
typedef struct TCALL_DISPATCHER_TARGET_STRUCT_TYPE_NAME_TAG(T)* TCALL_DISPATCHER_TARGET_HANDLE_NAME(T);         \
typedef struct TCALL_DISPATCHER_TARGET_STRUCT_TYPE_NAME_TAG(T)                                                  \
{                                                                                                               \
    DLIST_ENTRY anchor;                                                                                         \
    TCALL_DISPATCHER_LL_TARGET_FUNC(T) function_to_call;                                                        \
    void* call_context;                                                                                         \
} TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T);                                                                      \
typedef struct TCALL_DISPATCHER_STRUCT_TYPE_NAME_TAG(T)                                                         \
{                                                                                                               \
    SRW_LOCK_LL lock;                                                                                           \
    DLIST_ENTRY call_targets;                                                                                   \
} TCALL_DISPATCHER_TYPEDEF_NAME(T);                                                                             \

/*TCALL_DISPATCHER is-a THANDLE*/
/*given a type "T" TCALL_DISPATCHER_LL(T) expands to the name of the type. */
#define TCALL_DISPATCHER_LL(T) THANDLE(TCALL_DISPATCHER_TYPEDEF_NAME(T))

/*introduces a target handle for the call dispatcher type*/
#define TCALL_DISPATCHER_LL_TARGET_HANDLE(T) TCALL_DISPATCHER_TARGET_HANDLE_NAME(T)

/*introduces the target function type*/
#define TCALL_DISPATCHER_LL_TARGET_FUNC(T) TCALL_DISPATCHER_TARGET_FUNC_TYPE_NAME(T)

/*because TCALL_DISPATCHER is a THANDLE, all THANDLE's macro APIs are useable with TCALL_DISPATCHER.*/
/*the below are just shortcuts of THANDLE's public ones*/
#define TCALL_DISPATCHER_LL_INITIALIZE(T) THANDLE_INITIALIZE(TCALL_DISPATCHER_TYPEDEF_NAME(T))
#define TCALL_DISPATCHER_LL_ASSIGN(T) THANDLE_ASSIGN(TCALL_DISPATCHER_TYPEDEF_NAME(T))
#define TCALL_DISPATCHER_LL_MOVE(T) THANDLE_MOVE(TCALL_DISPATCHER_TYPEDEF_NAME(T))
#define TCALL_DISPATCHER_LL_INITIALIZE_MOVE(T) THANDLE_INITIALIZE_MOVE(TCALL_DISPATCHER_TYPEDEF_NAME(T))

/*introduces a new name for a function that returns a TCALL_DISPATCHER_LL(T)*/
#define TCALL_DISPATCHER_LL_CREATE_NAME(C) MU_C2(TCALL_DISPATCHER_LL_CREATE_, C)
#define TCALL_DISPATCHER_LL_CREATE(C) TCALL_DISPATCHER_LL_CREATE_NAME(C)

/*introduces a new name for the register_target function */
#define TCALL_DISPATCHER_LL_REGISTER_TARGET_NAME(C) MU_C2(TCALL_DISPATCHER_LL_REGISTER_TARGET_, C)
#define TCALL_DISPATCHER_LL_REGISTER_TARGET(C) TCALL_DISPATCHER_LL_REGISTER_TARGET_NAME(C)

/*introduces a new name for the unregister_target function */
#define TCALL_DISPATCHER_LL_UNREGISTER_TARGET_NAME(C) MU_C2(TCALL_DISPATCHER_LL_UNREGISTER_TARGET_, C)
#define TCALL_DISPATCHER_LL_UNREGISTER_TARGET(C) TCALL_DISPATCHER_LL_UNREGISTER_TARGET_NAME(C)

/*introduces a new name for the dispatch_call function */
#define TCALL_DISPATCHER_LL_DISPATCH_CALL_NAME(C) MU_C2(TCALL_DISPATCHER_LL_DISPATCH_CALL_, C)
#define TCALL_DISPATCHER_LL_DISPATCH_CALL(C) TCALL_DISPATCHER_LL_DISPATCH_CALL_NAME(C)

/*introduces a function declaration for tcall_dispatcher_create*/
#define TCALL_DISPATCHER_LL_CREATE_DECLARE(C, T) MOCKABLE_FUNCTION(, TCALL_DISPATCHER_LL(T), TCALL_DISPATCHER_LL_CREATE(C));

/*introduces a function declaration for tcall_dispatcher_register_target*/
#define TCALL_DISPATCHER_LL_REGISTER_TARGET_DECLARE(C, T) MOCKABLE_FUNCTION(, TCALL_DISPATCHER_LL_TARGET_HANDLE(T), TCALL_DISPATCHER_LL_REGISTER_TARGET(C), TCALL_DISPATCHER_LL(T), tcall_dispatcher, TCALL_DISPATCHER_LL_TARGET_FUNC(T), function_to_call, void*, call_context);

/*introduces a function declaration for tcall_dispatcher_unregister_target*/
#define TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DECLARE(C, T) MOCKABLE_FUNCTION(, int, TCALL_DISPATCHER_LL_UNREGISTER_TARGET(C), TCALL_DISPATCHER_LL(T), tcall_dispatcher, TCALL_DISPATCHER_LL_TARGET_HANDLE(T), call_target);

/*introduces a function declaration for tcall_dispatcher_dispatch_call*/
#define TCALL_DISPATCHER_LL_DISPATCH_CALL_DECLARE(C, T, ...) MOCKABLE_FUNCTION(, int, TCALL_DISPATCHER_LL_DISPATCH_CALL(C), TCALL_DISPATCHER_LL(T), tcall_dispatcher MU_FOR_EACH_2(TCALL_DISPATCHER_ARG_IN_MOCKABLE_SIGNATURE, __VA_ARGS__));

/*introduces a name for the function that free's a TCALL_DISPATCHER when it's ref count got to 0*/
#define TCALL_DISPATCHER_LL_FREE_NAME(C) MU_C2(TCALL_DISPATCHER_LL_FREE_, C)

/*introduces a function definition for freeing the allocated resources for a TCALL_DISPATCHER*/
#define TCALL_DISPATCHER_LL_FREE_DEFINE(C, T) \
static void TCALL_DISPATCHER_LL_FREE_NAME(C)(TCALL_DISPATCHER_TYPEDEF_NAME(T)* tcall_dispatcher)                                                                    \
{                                                                                                                                                                   \
    if (tcall_dispatcher == NULL)                                                                                                                                   \
    {                                                                                                                                                               \
        LogError("invalid arguments " MU_TOSTRING(TCALL_DISPATCHER_TYPEDEF_NAME(T)) "* tcall_dispatcher=%p",                                                        \
            tcall_dispatcher);                                                                                                                                      \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_026: [ TCALL_DISPATCHER_LL_FREE_{T} shall call srw_lock_ll_deinit to de-initialize the lock. ]*/                           \
        srw_lock_ll_deinit(&tcall_dispatcher->lock);                                                                                                                \
        DLIST_ENTRY* current_entry = tcall_dispatcher->call_targets.Flink;                                                                                          \
        while (current_entry != &tcall_dispatcher->call_targets)                                                                                                    \
        {                                                                                                                                                           \
            /* Codes_SRS_TCALL_DISPATCHER_01_027: [ For each call target in the list, TCALL_DISPATCHER_LL_FREE_{T} shall free the resources associated with the call target. ]*/ \
            TCALL_DISPATCHER_TARGET_HANDLE_NAME(T) call_target_handle = CONTAINING_RECORD(current_entry, TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T), anchor);          \
            DList_RemoveEntryList(current_entry);                                                                                                                   \
            free(call_target_handle);                                                                                                                               \
            current_entry = tcall_dispatcher->call_targets.Flink;                                                                                                   \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
}                                                                                                                                                                   \

/*introduces a function definition for tcall_dispatcher_create*/
#define TCALL_DISPATCHER_LL_CREATE_DEFINE(C, T)                                                                                                                     \
TCALL_DISPATCHER_LL(T) TCALL_DISPATCHER_LL_CREATE(C)(void)                                                                                                          \
{                                                                                                                                                                   \
    /* Codes_SRS_TCALL_DISPATCHER_01_001: [ TCALL_DISPATCHER_CREATE(T) shall call THANDLE_MALLOC to allocate the result. ]*/                                        \
    TCALL_DISPATCHER_TYPEDEF_NAME(T)* result = THANDLE_MALLOC(TCALL_DISPATCHER_TYPEDEF_NAME(C))(TCALL_DISPATCHER_LL_FREE_NAME(C));                                  \
    if(result == NULL)                                                                                                                                              \
    {                                                                                                                                                               \
        LogError("failure in " MU_TOSTRING(THANDLE_MALLOC) "(" MU_TOSTRING(TCALL_DISPATCHER_TYPEDEF_NAME(T)) "=%zu)", sizeof(TCALL_DISPATCHER_TYPEDEF_NAME(T)));    \
        /*return as is*/                                                                                                                                            \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_002: [ TCALL_DISPATCHER_CREATE(T) shall call srw_lock_ll_init to initialize the lock used by the TCALL_DISPATCHER instance. ]*/ \
        if (srw_lock_ll_init(&result->lock) != 0)                                                                                                                   \
        {                                                                                                                                                           \
            /* Codes_SRS_TCALL_DISPATCHER_01_005: [ If there are any failures then TCALL_DISPATCHER_CREATE(T) shall fail and return NULL. ]*/                       \
            LogError("srw_lock_ll_init failed");                                                                                                                    \
        }                                                                                                                                                           \
        else                                                                                                                                                        \
        {                                                                                                                                                           \
            /* Codes_SRS_TCALL_DISPATCHER_01_003: [ TCALL_DISPATCHER_CREATE(T) shall call DList_InitializeListHead to initialize the doubly linked list that holds the target call registrations. ]*/ \
            DList_InitializeListHead(&result->call_targets);                                                                                                        \
            /*return as is*/                                                                                                                                        \
            /* Codes_SRS_TCALL_DISPATCHER_01_004: [ TCALL_DISPATCHER_CREATE(T) shall succeed and return a non-NULL value. ]*/                                       \
            goto all_ok;                                                                                                                                            \
        }                                                                                                                                                           \
        THANDLE_FREE(TCALL_DISPATCHER_TYPEDEF_NAME(C))(result);                                                                                                     \
        result = NULL;                                                                                                                                              \
    }                                                                                                                                                               \
all_ok:                                                                                                                                                             \
    return result;                                                                                                                                                  \
}

/*introduces a function definition for tcall_dispatcher_register_target*/
#define TCALL_DISPATCHER_LL_REGISTER_TARGET_DEFINE(C, T)                                                                                                            \
TCALL_DISPATCHER_LL_TARGET_HANDLE(T) TCALL_DISPATCHER_LL_REGISTER_TARGET(C)(TCALL_DISPATCHER_LL(T) tcall_dispatcher, TCALL_DISPATCHER_LL_TARGET_FUNC(T) function_to_call, void* call_context) \
{                                                                                                                                                                   \
    TCALL_DISPATCHER_LL_TARGET_HANDLE(T) result;                                                                                                                    \
    if (                                                                                                                                                            \
        /* Codes_SRS_TCALL_DISPATCHER_01_006: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ] */               \
        (tcall_dispatcher == NULL) ||                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_007: [ If function_to_call is NULL then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ]*/                \
        (function_to_call == NULL)                                                                                                                                  \
       )                                                                                                                                                            \
    {                                                                                                                                                               \
        LogError("Invalid arguments TCALL_DISPATCHER_LL(T) tcall_dispatcher=%p, TCALL_DISPATCHER_LL_TARGET_FUNC(T) function_to_call=%p, void* call_context=%p",     \
            tcall_dispatcher, function_to_call, call_context);                                                                                                      \
        result = NULL;                                                                                                                                              \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_008: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall allocate memory for a TCALL_DISPATCHER_TARGET_HANDLE(T) that holds function_to_call and context. ]*/ \
        result = malloc(sizeof(TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T)));                                                                                           \
        if (result == NULL)                                                                                                                                         \
        {                                                                                                                                                           \
            /* Codes_SRS_TCALL_DISPATCHER_01_013: [ If there are any failures then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ] */             \
            LogError("malloc(sizeof(TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(" MU_TOSTRING(T) "))=%zu) failed", sizeof(TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T)));       \
            /* return as is */                                                                                                                                      \
        }                                                                                                                                                           \
        else                                                                                                                                                        \
        {                                                                                                                                                           \
            result->function_to_call = function_to_call;                                                                                                            \
            result->call_context = call_context;                                                                                                                    \
                                                                                                                                                                    \
            /* Codes_SRS_TCALL_DISPATCHER_01_009: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall acquire exclusivly the lock. ]*/                                      \
            srw_lock_ll_acquire_exclusive((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                   \
                                                                                                                                                                    \
            /* Codes_SRS_TCALL_DISPATCHER_01_010: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall add the new TCALL_DISPATCHER_TARGET_HANDLE(T) containing function_to_call and context in the doubly linked list. ]*/ \
            DList_InsertTailList((DLIST_ENTRY*)&tcall_dispatcher->call_targets, &result->anchor);                                                                   \
                                                                                                                                                                    \
            /* Codes_SRS_TCALL_DISPATCHER_01_011: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall release the lock. ]*/                                                 \
            srw_lock_ll_release_exclusive((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                   \
            /* Codes_SRS_TCALL_DISPATCHER_01_012: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall succeed and return the TCALL_DISPATCHER_TARGET_HANDLE(T). ] */        \
            /* return as is */                                                                                                                                      \
        }                                                                                                                                                           \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}

/*introduces a function definition for tcall_dispatcher_unregister_target*/
#define TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DEFINE(C, T)                                                                                                          \
int TCALL_DISPATCHER_LL_UNREGISTER_TARGET(C)(TCALL_DISPATCHER_LL(T) tcall_dispatcher, TCALL_DISPATCHER_LL_TARGET_HANDLE(T) call_target)                             \
{                                                                                                                                                                   \
    int result;                                                                                                                                                     \
    if (                                                                                                                                                            \
        /* Codes_SRS_TCALL_DISPATCHER_01_014: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall fail and return a non-zero value. ]*/  \
        (tcall_dispatcher == NULL) ||                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_015: [ If call_target is NULL then TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall fail and return a non-zero value. ]*/       \
        (call_target == NULL)                                                                                                                                       \
       )                                                                                                                                                            \
    {                                                                                                                                                               \
        LogError("Invalid arguments TCALL_DISPATCHER_LL(" MU_TOSTRING(T) ") tcall_dispatcher=%p, TCALL_DISPATCHER_LL_TARGET_HANDLE(" MU_TOSTRING(T) ") call_target=%p", \
            tcall_dispatcher, call_target);                                                                                                                         \
        result = MU_FAILURE;                                                                                                                                        \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_016: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall acquire exclusivly the lock. ]*/                                        \
        srw_lock_ll_acquire_exclusive((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                       \
        /* Codes_SRS_TCALL_DISPATCHER_01_017: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall remove from the doubly linked list the call target call_target. ]*/     \
        DList_RemoveEntryList(&call_target->anchor);                                                                                                                \
        /* Codes_SRS_TCALL_DISPATCHER_01_018: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall release the lock. ]*/                                                   \
        srw_lock_ll_release_exclusive((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                       \
        /* Codes_SRS_TCALL_DISPATCHER_01_019: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall free the call_target resources. ]*/                                     \
        free(call_target);                                                                                                                                          \
        /* Codes_SRS_TCALL_DISPATCHER_01_020: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall succeed and return 0. ]*/                                               \
        result = 0;                                                                                                                                                 \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}

/*introduces a function definition for tcall_dispatcher_unregister_target*/
#define TCALL_DISPATCHER_LL_DISPATCH_CALL_DEFINE(C, T, ...)                                                                                                         \
int TCALL_DISPATCHER_LL_DISPATCH_CALL(C)(TCALL_DISPATCHER_LL(T) tcall_dispatcher MU_FOR_EACH_2(TCALL_DISPATCHER_ARG_IN_SIGNATURE, __VA_ARGS__))                     \
{                                                                                                                                                                   \
    int result;                                                                                                                                                     \
    if (tcall_dispatcher == NULL)                                                                                                                                   \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_021: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_DISPATCH_CALL(T) shall fail and return a non-zero value. ]*/      \
        LogError("Invalid arguments TCALL_DISPATCHER_LL(" MU_TOSTRING(T) ") tcall_dispatcher=%p, ...",                                                              \
            tcall_dispatcher);                                                                                                                                      \
        result = MU_FAILURE;                                                                                                                                        \
    }                                                                                                                                                               \
    else                                                                                                                                                            \
    {                                                                                                                                                               \
        /* Codes_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/                             \
        srw_lock_ll_acquire_shared((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                          \
        DLIST_ENTRY* current_entry = tcall_dispatcher->call_targets.Flink;                                                                                          \
        while (current_entry != &tcall_dispatcher->call_targets)                                                                                                    \
        {                                                                                                                                                           \
            TCALL_DISPATCHER_TARGET_HANDLE_NAME(T) call_target_handle = CONTAINING_RECORD(current_entry, TCALL_DISPATCHER_TARGET_TYPEDEF_NAME(T), anchor);          \
            /* Codes_SRS_TCALL_DISPATCHER_01_023: [ For each call target that was registered, TCALL_DISPATCHER_DISPATCH_CALL(T) shall call the function_to_call with the associated context and the parameters in .... ]*/ \
            call_target_handle->function_to_call(call_target_handle->call_context MU_FOR_EACH_2(TCALL_DISPATCHER_ARG_VALUE_IN_CALL, __VA_ARGS__));                  \
            current_entry = current_entry->Flink;                                                                                                                   \
        }                                                                                                                                                           \
        /* Codes_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/                                                       \
        srw_lock_ll_release_shared((SRW_LOCK_LL*)&tcall_dispatcher->lock);                                                                                          \
        /* Codes_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/                                                   \
        result = 0;                                                                                                                                                 \
    }                                                                                                                                                               \
    return result;                                                                                                                                                  \
}

/*macro to be used in headers*/                                                                                                 \
#define TCALL_DISPATCHER_LL_TYPE_DECLARE(C, T, ...)                                                                             \
    /*hint: have TCALL_DISPATCHER_DEFINE_CALL_TYPE(T) before TCALL_DISPATCHER_LL_TYPE_DECLARE*/                                 \
    THANDLE_LL_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(C), TCALL_DISPATCHER_TYPEDEF_NAME(T))                                 \
    TCALL_DISPATCHER_LL_CREATE_DECLARE(C, T)                                                                                    \
    TCALL_DISPATCHER_LL_REGISTER_TARGET_DECLARE(C, T)                                                                           \
    TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DECLARE(C, T)                                                                         \
    TCALL_DISPATCHER_LL_DISPATCH_CALL_DECLARE(C, T, __VA_ARGS__)                                                                \

/*macro to be used in .c*/                                                                                                      \
#define TCALL_DISPATCHER_LL_TYPE_DEFINE(C, T, ...)                                                                              \
    /*hint: have THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(T)) before TCALL_DISPATCHER_LL_TYPE_DEFINE*/                 \
    TCALL_DISPATCHER_LL_FREE_DEFINE(C, T)                                                                                       \
    TCALL_DISPATCHER_LL_CREATE_DEFINE(C, T)                                                                                     \
    TCALL_DISPATCHER_LL_REGISTER_TARGET_DEFINE(C, T)                                                                            \
    TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DEFINE(C, T)                                                                          \
    TCALL_DISPATCHER_LL_DISPATCH_CALL_DEFINE(C, T, __VA_ARGS__)                                                                 \

#endif  /*TCALL_DISPATCHER_LL_H*/
