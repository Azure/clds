// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// Precompiled header for lock_free_set_ut

#ifndef LOCK_FREE_SET_UT_PCH_H
#define LOCK_FREE_SET_UT_PCH_H

#include <stdbool.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_bool.h"
#include "c_pal/interlocked.h"

#include "umock_c/umock_c_ENABLE_MOCKS.h" // ============================== ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "umock_c/umock_c_DISABLE_MOCKS.h" // ============================== DISABLE_MOCKS

#include "real_gballoc_hl.h"

#include "clds/lock_free_set.h"

#endif // LOCK_FREE_SET_UT_PCH_H
