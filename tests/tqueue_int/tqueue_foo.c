// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdint.h>

#include "c_pal/thandle.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/tqueue.h"

#include "tqueue_foo.h"

// This is the queue used for most of the tests
THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(FOO));
TQUEUE_TYPE_DEFINE(FOO);
