// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Precompiled header for clds_reals_ut

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"

#define ENABLE_MOCKS

#if WIN32
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"
#endif
#include "clds/clds_hash_table.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_st_hash_set.h"
#include "clds/lock_free_set.h"
#include "clds/mpsc_lock_free_queue.h"
#include "clds/inactive_hp_thread_queue.h"

#undef ENABLE_MOCKS

#if WIN32
#include "../tests/reals/real_clds_hazard_pointers.h"
#include "../tests/reals/real_clds_hazard_pointers_thread_helper.h"
#endif
#include "../tests/reals/real_clds_hash_table.h"
#include "../tests/reals/real_clds_singly_linked_list.h"
#include "../tests/reals/real_clds_sorted_list.h"
#include "../tests/reals/real_clds_st_hash_set.h"
#include "../tests/reals/real_lock_free_set.h"
#include "../tests/reals/real_mpsc_lock_free_queue.h"
#include "../tests/reals/real_inactive_hp_thread_queue.h"
