// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Precompiled header for lru_cache_ut

#include <inttypes.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"
#include "umock_c/umock_c_negative_tests.h"

// TODO: This should go back to mocks: 
// Task 25774695: Fix mocking for interlocked when using reals hazard pointers
#include "c_pal/interlocked.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/srw_lock_ll.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hash_table.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_srw_lock_ll.h"
#include "real_doublylinkedlist.h"

#include "../reals/real_interlocked.h"
#include "../reals/real_clds_hash_table.h"
#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_hazard_pointers_thread_helper.h"

#include "real_thread_notifications_dispatcher.h"


#include "clds/lru_cache.h"
