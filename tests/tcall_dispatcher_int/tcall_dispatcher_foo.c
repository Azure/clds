// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdint.h>

#include "c_pal/thandle.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/tcall_dispatcher.h"

#include "tcall_dispatcher_foo.h"

// This is the call dispatcher used for most of the tests
THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO));
TCALL_DISPATCHER_TYPE_DEFINE(FOO, int32_t, x);
