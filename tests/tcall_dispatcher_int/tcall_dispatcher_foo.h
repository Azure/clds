// Copyright (c) Microsoft. All rights reserved.

#ifndef TCALL_DISPATCHER_FOO_H
#define TCALL_DISPATCHER_FOO_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "c_pal/thandle.h"

#include "clds/tcall_dispatcher.h"

// This is the call dispatcher used for most of the tests
TCALL_DISPATCHER_DEFINE_CALL_TYPE(FOO, int32_t, x);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO))
TCALL_DISPATCHER_TYPE_DECLARE(FOO, int32_t, x);

#endif // TCALL_DISPATCHER_FOO_H
