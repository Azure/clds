// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef TCALL_DISPATCHER_H
#define TCALL_DISPATCHER_H

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle_ll.h"
#include "clds/tcall_dispatcher_ll.h"

#include "umock_c/umock_c_prod.h"

/*TCALL_DISPATCHER is-a THANDLE.*/
/*given a type "T" TCALL_DISPATCHER(T) expands to the name of the type. */
#define TCALL_DISPATCHER(T)

/*macro to be used in headers*/                                                                                               \
#define TCALL_DISPATCHER_TYPE_DECLARE(T, ...)                                                                                 \

#define TCALL_DISPATCHER_TYPE_DEFINE(T, ...)                                                                                  \

#endif // TCALL_DISPATCHER_H
