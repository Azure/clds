// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef TQUEUE_H
#define TQUEUE_H

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle_ll.h"
#include "clds/tqueue_ll.h"

#include "umock_c/umock_c_prod.h"

/*TQUEUE is-a THANDLE.*/
/*given a type "T" TQUEUE(T) expands to the name of the type. */
#define TQUEUE(T) TQUEUE_LL(T)

#define TQUEUE_CREATE_DECLARE(T) TQUEUE_LL_CREATE_DECLARE(T, T)
#define TQUEUE_PUSH_DECLARE(T) TQUEUE_LL_PUSH_DECLARE(T, T)
#define TQUEUE_POP_DECLARE(T) TQUEUE_LL_POP_DECLARE(T, T)

#define TQUEUE_CREATE_DEFINE(T) TQUEUE_LL_CREATE_DEFINE(T, T)
#define TQUEUE_PUSH_DEFINE(T) TQUEUE_LL_PUSH_DEFINE(T, T)
#define TQUEUE_POP_DEFINE(T) TQUEUE_LL_POP_DEFINE(T, T)

#define TQUEUE_FREE_DEFINE(T) TQUEUE_LL_FREE_DEFINE(T, T)

#define TQUEUE_CREATE(C) TQUEUE_LL_CREATE(C)

#define TQUEUE_PUSH(C) TQUEUE_LL_PUSH(C)
#define TQUEUE_POP(C) TQUEUE_LL_POP(C)

#define TQUEUE_INITIALIZE(T) TQUEUE_LL_INITIALIZE(T)
#define TQUEUE_ASSIGN(T) TQUEUE_LL_ASSIGN(T)
#define TQUEUE_MOVE(T) TQUEUE_LL_MOVE(T)
#define TQUEUE_INITIALIZE_MOVE(T) TQUEUE_LL_INITIALIZE_MOVE(T)

#define TQUEUE_TARGET_HANDLE(T) TQUEUE_LL_TARGET_HANDLE(T)
#define TQUEUE_TARGET_FUNC(T) TQUEUE_LL_TARGET_FUNC(T)

/*macro to be used in headers*/                                                                                     \
#define TQUEUE_TYPE_DECLARE(T, ...)                                                                                 \
    /*hint: have TQUEUE_DEFINE_TYPE(T) before TQUEUE_TYPE_DECLARE                               */                  \
    /*hint: have THANDLE_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(T)) before TQUEUE_TYPE_DECLARE               */           \
    TQUEUE_CREATE_DECLARE(T)                                                                                        \
    TQUEUE_PUSH_DECLARE(T)                                                                                          \
    TQUEUE_POP_DECLARE(T)                                                                                           \

#define TQUEUE_TYPE_DEFINE(T, ...)                                                                                  \
    /*hint: have THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(T)) before  TQUEUE_TYPE_DEFINE                */           \
    TQUEUE_FREE_DEFINE(T)                                                                                           \
    TQUEUE_CREATE_DEFINE(T)                                                                                         \
    TQUEUE_PUSH_DEFINE(T)                                                                                           \
    TQUEUE_POP_DEFINE(T)                                                                                            \

#endif // TQUEUE_H
