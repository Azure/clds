// Copyright (c) Microsoft. All rights reserved.

#ifndef TQUEUE_FOO_H
#define TQUEUE_FOO_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "c_pal/thandle.h"

#include "clds/tqueue.h"

typedef struct FOO_TAG
{
    int64_t x;
} FOO;

// This is the call dispatcher used for most of the tests
TQUEUE_DEFINE_STRUCT_TYPE(FOO);
THANDLE_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(FOO))
TQUEUE_TYPE_DECLARE(FOO);

#endif // TQUEUE_FOO_H
