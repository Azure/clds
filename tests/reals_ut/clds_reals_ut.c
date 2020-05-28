// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"

#define ENABLE_MOCKS

#if defined _MSC_VER
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hash_table.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_st_hash_set.h"
#endif
#include "clds/lock_free_set.h"

#undef ENABLE_MOCKS

#if defined _MSC_VER
#include "../tests/reals/real_clds_hazard_pointers.h"
#include "../tests/reals/real_clds_hash_table.h"
#include "../tests/reals/real_clds_singly_linked_list.h"
#include "../tests/reals/real_clds_sorted_list.h"
#include "../tests/reals/real_clds_st_hash_set.h"
#endif
#include "../tests/reals/real_lock_free_set.h"

BEGIN_TEST_SUITE(clds_reals_ut)

// this test makes sure that the mappings work
// (there is a real_ function corresponding to the original)
TEST_FUNCTION(check_all_c_util_reals)
{
    // arrange

    // act
#if defined _MSC_VER
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SINGLY_LINKED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SORTED_LIST_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
#endif
    REGISTER_LOCK_FREE_SET_GLOBAL_MOCK_HOOKS();

    // assert
    // no explicit assert, if it builds it works
}

END_TEST_SUITE(clds_reals_ut)
