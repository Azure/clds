// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"

#define ENABLE_MOCKS

#include "clds/clds_hazard_pointers.h"
#if WIN32
#include "clds/clds_hazard_pointers_thread_helper.h"
#include "clds/tcall_dispatcher_thread_notification_call.h"
#include "clds/thread_notifications_dispatcher.h"
#endif
#include "clds/clds_hash_table.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_st_hash_set.h"
#include "clds/lock_free_set.h"
#include "clds/mpsc_lock_free_queue.h"

#undef ENABLE_MOCKS

#include "../tests/reals/real_clds_hazard_pointers.h"
#if WIN32
#include "../tests/reals/real_clds_hazard_pointers_thread_helper.h"
#endif
#include "../tests/reals/real_clds_hash_table.h"
#include "../tests/reals/real_clds_singly_linked_list.h"
#include "../tests/reals/real_clds_sorted_list.h"
#include "../tests/reals/real_clds_st_hash_set.h"
#include "../tests/reals/real_lock_free_set.h"
#include "../tests/reals/real_mpsc_lock_free_queue.h"
#include "../tests/reals/real_tcall_dispatcher_thread_notification_call.h"
#include "../tests/reals/real_thread_notifications_dispatcher.h"

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

// this test makes sure that the mappings work
// (there is a real_ function corresponding to the original)
TEST_FUNCTION(check_all_clds_reals)
{
    // arrange

    // act
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
#if WIN32
    REGISTER_CLDS_HAZARD_POINTERS_THREAD_HELPER_GLOBAL_MOCK_HOOKS();
    REGISTER_TCALL_DISPATCHER_THREAD_NOTIFICATION_CALL_GLOBAL_MOCK_HOOK();
    REGISTER_THREAD_NOTIFICATIONS_DISPATCHER_GLOBAL_MOCK_HOOK();
#endif
    REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SINGLY_LINKED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SORTED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_LOCK_FREE_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_MPSC_LOCK_FREE_QUEUE_GLOBAL_MOCK_HOOKS();

    // assert
    // no explicit assert, if it builds it works
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
