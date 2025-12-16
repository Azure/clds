// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "clds_reals_ut_pch.h"


BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

// this test makes sure that the mappings work
// (there is a real_ function corresponding to the original)
TEST_FUNCTION(check_all_clds_reals) // no-srs
{
    // arrange

    // act
#if WIN32
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_THREAD_HELPER_GLOBAL_MOCK_HOOKS();
#endif
    REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SINGLY_LINKED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SORTED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_LOCK_FREE_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_MPSC_LOCK_FREE_QUEUE_GLOBAL_MOCK_HOOKS();
    REGISTER_INACTIVE_HP_THREAD_QUEUE_GLOBAL_MOCK_HOOK();

    // assert
    // no explicit assert, if it builds it works
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
