// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef TCALL_DISPATCHER_H
#define TCALL_DISPATCHER_H

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle_ll.h"
#include "clds/tcall_dispatcher_ll.h"

#include "umock_c/umock_c_prod.h"

/*TCALL_DISPATCHER is-a THANDLE.*/
/*given a type "T" TCALL_DISPATCHER(T) expands to the name of the type. */
#define TCALL_DISPATCHER(T) TCALL_DISPATCHER_LL(T)

#define TCALL_DISPATCHER_CREATE_DECLARE(T) TCALL_DISPATCHER_LL_CREATE_DECLARE(T, T)
#define TCALL_DISPATCHER_REGISTER_TARGET_DECLARE(T) TCALL_DISPATCHER_LL_REGISTER_TARGET_DECLARE(T, T)
#define TCALL_DISPATCHER_UNREGISTER_TARGET_DECLARE(T) TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DECLARE(T, T)
#define TCALL_DISPATCHER_DISPATCH_CALL_DECLARE(T, ...) TCALL_DISPATCHER_LL_DISPATCH_CALL_DECLARE(T, T, __VA_ARGS__)

#define TCALL_DISPATCHER_CREATE_DEFINE(T) TCALL_DISPATCHER_LL_CREATE_DEFINE(T, T)
#define TCALL_DISPATCHER_REGISTER_TARGET_DEFINE(T) TCALL_DISPATCHER_LL_REGISTER_TARGET_DEFINE(T, T)
#define TCALL_DISPATCHER_UNREGISTER_TARGET_DEFINE(T) TCALL_DISPATCHER_LL_UNREGISTER_TARGET_DEFINE(T, T)
#define TCALL_DISPATCHER_DISPATCH_CALL_DEFINE(T, ...) TCALL_DISPATCHER_LL_DISPATCH_CALL_DEFINE(T, T, __VA_ARGS__)

#define TCALL_DISPATCHER_FREE_DEFINE(T) TCALL_DISPATCHER_LL_FREE_DEFINE(T, T)

#define TCALL_DISPATCHER_CREATE(C) TCALL_DISPATCHER_LL_CREATE(C)

#define TCALL_DISPATCHER_REGISTER_TARGET(C) TCALL_DISPATCHER_LL_REGISTER_TARGET(C)
#define TCALL_DISPATCHER_UNREGISTER_TARGET(C) TCALL_DISPATCHER_LL_UNREGISTER_TARGET(C)
#define TCALL_DISPATCHER_DISPATCH_CALL(C) TCALL_DISPATCHER_LL_DISPATCH_CALL(C)

#define TCALL_DISPATCHER_INITIALIZE(T) TCALL_DISPATCHER_LL_INITIALIZE(T)
#define TCALL_DISPATCHER_ASSIGN(T) TCALL_DISPATCHER_LL_ASSIGN(T)
#define TCALL_DISPATCHER_MOVE(T) TCALL_DISPATCHER_LL_MOVE(T)
#define TCALL_DISPATCHER_INITIALIZE_MOVE(T) TCALL_DISPATCHER_LL_INITIALIZE_MOVE(T)

#define TCALL_DISPATCHER_TARGET_HANDLE(T) TCALL_DISPATCHER_LL_TARGET_HANDLE(T)
#define TCALL_DISPATCHER_TARGET_FUNC(T) TCALL_DISPATCHER_LL_TARGET_FUNC(T)

/*macro to be used in headers*/                                                                                               \
#define TCALL_DISPATCHER_TYPE_DECLARE(T, ...)                                                                                 \
    /*hint: have TCALL_DISPATCHER_DEFINE_CALL_TYPE(T) before TCALL_DISPATCHER_TYPE_DECLARE                               */   \
    /*hint: have THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(T)) before TCALL_DISPATCHER_TYPE_DECLARE               */ \
    TCALL_DISPATCHER_CREATE_DECLARE(T)                                                                                        \
    TCALL_DISPATCHER_REGISTER_TARGET_DECLARE(T)                                                                               \
    TCALL_DISPATCHER_UNREGISTER_TARGET_DECLARE(T)                                                                             \
    TCALL_DISPATCHER_DISPATCH_CALL_DECLARE(T, __VA_ARGS__)                                                                    \

#define TCALL_DISPATCHER_TYPE_DEFINE(T, ...)                                                                                  \
    /*hint: have THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(T)) before  TCALL_DISPATCHER_TYPE_DEFINE                */ \
    TCALL_DISPATCHER_FREE_DEFINE(T)                                                                                           \
    TCALL_DISPATCHER_CREATE_DEFINE(T)                                                                                         \
    TCALL_DISPATCHER_REGISTER_TARGET_DEFINE(T)                                                                                \
    TCALL_DISPATCHER_UNREGISTER_TARGET_DEFINE(T)                                                                              \
    TCALL_DISPATCHER_DISPATCH_CALL_DEFINE(T, __VA_ARGS__)                                                                     \

#endif // TCALL_DISPATCHER_H
