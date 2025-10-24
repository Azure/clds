// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// Precompiled header for clds_hazard_pointers_thread_helper_ut

#ifndef CLDS_HAZARD_POINTERS_THREAD_HELPER_UT_PCH_H
#define CLDS_HAZARD_POINTERS_THREAD_HELPER_UT_PCH_H

#include <stdlib.h>
#include <stddef.h>

#include "windows.h"

#include "macro_utils/macro_utils.h"

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes.h"
#include "umock_c/umocktypes_windows.h"

#include "c_pal/interlocked.h" /*included for mocking reasons - it will prohibit creation of mocks belonging to interlocked.h - at the moment verified through int tests - this is porting legacy code, temporary solution*/

#include "umock_c/umock_c_ENABLE_MOCKS.h" // ============================== ENABLE_MOCKS
#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/thandle.h"

#include "c_pal/ps_util.h"

#include "c_util/thread_notifications_dispatcher.h"
#include "clds/clds_hazard_pointers.h"
#include "umock_c/umock_c_DISABLE_MOCKS.h" // ============================== DISABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_tcall_dispatcher_thread_notification_call.h"

#include "clds/clds_hazard_pointers_thread_helper.h"

#endif // CLDS_HAZARD_POINTERS_THREAD_HELPER_UT_PCH_H
