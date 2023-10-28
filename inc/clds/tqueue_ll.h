// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef TQUEUE_LL_H
#define TQUEUE_LL_H

#include "c_pal/thandle_ll.h"

#include "umock_c/umock_c_prod.h"

#define TQUEUE_POP_RESULT_VALUES \
    TQUEUE_POP_OK, \
    TQUEUE_POP_ERROR, \
    TQUEUE_POP_QUEUE_EMPTY

MU_DEFINE_ENUM(TQUEUE_POP_RESULT, TQUEUE_POP_RESULT_VALUES);

#define TQUEUE_POP_IF_RESULT_VALUES \
    TQUEUE_POP_IF_OK, \
    TQUEUE_POP_IF_ERROR, \
    TQUEUE_POP_IF_QUEUE_EMPTY

MU_DEFINE_ENUM(TQUEUE_POP_IF_RESULT, TQUEUE_POP_IF_RESULT_VALUES);

/*macro to be used in headers*/                                                                                         \
#define TQUEUE_LL_TYPE_DECLARE(C, T, ...)                                                                               \
    /*hint: have TQUEUE_DEFINE_STRUCT_TYPE(T) before TQUEUE_LL_TYPE_DECLARE*/                                           \

/*macro to be used in .c*/                                                                                              \
#define TQUEUE_LL_TYPE_DEFINE(C, T, ...)                                                                                \

#endif  /*TQUEUE_LL_H*/
