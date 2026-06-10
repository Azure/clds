// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "macro_utils/macro_utils.h"

#include "testrunnerswitcher.h"

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/interlocked.h"
#include "c_pal/threadapi.h"

#include "clds/clds_hazard_pointers.h"

// Number of milliseconds to wait after unregistering a thread so that the hazard pointers cleanup
// worker has time to move it onto the inactive threads queue (the worst case for the leak this
// test guards against - the unregistered thread is fully gone before the instance is destroyed).
#define CLEANUP_WORKER_GRACE_MS 1500

// Counts how many times the test reclaim callback ran. The reclaim callback signature has no
// context, so a file-scope atomic is used and reset at the start of each test.
static volatile_atomic int32_t g_reclaim_count;

static void test_reclaim(void* node)
{
    // free the node so that the absence of a reclaim (the bug) shows up as a leak under VLD/ASAN,
    // and bump the counter so the leak is also detectable as a deterministic assertion failure.
    free(node);
    (void)interlocked_increment(&g_reclaim_count);
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, gballoc_hl_init(NULL, NULL));
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    (void)interlocked_exchange(&g_reclaim_count, 0);
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

// Control: with no hazard pointer held on the node, a reclaim must run immediately (the default
// reclaim threshold is 1). This proves the reclaim wiring is correct, so a 0 count in the bug test
// below is meaningful rather than a broken harness.
TEST_FUNCTION(reclaim_runs_immediately_when_no_hazard_pointer_is_held)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(thread);
    int* node = malloc(sizeof(int));
    ASSERT_IS_NOT_NULL(node);

    // act
    // no hazard pointer is held on node, so this reclaim should run right away
    clds_hazard_pointers_reclaim(thread, node, test_reclaim);

    // assert
    ASSERT_ARE_EQUAL(int32_t, 1, interlocked_add(&g_reclaim_count, 0));

    // cleanup
    clds_hazard_pointers_unregister_thread(thread);
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

// Regression for the leak fixed by Task #36604983: a node retired by a thread that then
// unregisters, while another thread still holds a hazard pointer on it, must still be reclaimed.
// Before the fix the retiring thread's reclaim list was dropped when its data was freed, so the
// node (and its reclaim callback) leaked forever.
/* Tests_SRS_CLDS_HAZARD_POINTERS_42_001: [ clds_hazard_pointers_unregister_thread shall hand off any entries still on the thread's reclaim list to the global pending reclaim list of the hazard pointers instance. ]*/
/* Tests_SRS_CLDS_HAZARD_POINTERS_42_003: [ clds_hazard_pointers_destroy shall reclaim all entries remaining on the global pending reclaim list. ]*/
TEST_FUNCTION(node_retired_by_unregistering_thread_while_held_is_reclaimed)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE retiring_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(retiring_thread);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE holding_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(holding_thread);
    int* node = malloc(sizeof(int));
    ASSERT_IS_NOT_NULL(node);

    // holding_thread protects node with a hazard pointer so it cannot be reclaimed yet
    CLDS_HAZARD_POINTER_RECORD_HANDLE held_record = clds_hazard_pointers_acquire(holding_thread, node);
    ASSERT_IS_NOT_NULL(held_record);

    // act
    // retiring_thread retires node while holding_thread still protects it: node is stranded on
    // retiring_thread's reclaim list
    clds_hazard_pointers_reclaim(retiring_thread, node, test_reclaim);
    ASSERT_ARE_EQUAL(int32_t, 0, interlocked_add(&g_reclaim_count, 0), "node must not be reclaimed while a hazard pointer is held");

    // retiring_thread unregisters and is moved onto the inactive queue by the cleanup worker
    clds_hazard_pointers_unregister_thread(retiring_thread);
    ThreadAPI_Sleep(CLEANUP_WORKER_GRACE_MS);

    // the last holder releases and unregisters; node now has no protector
    clds_hazard_pointers_release(holding_thread, held_record);
    clds_hazard_pointers_unregister_thread(holding_thread);

    clds_hazard_pointers_destroy(clds_hazard_pointers);

    // assert
    // the node retired by the now-gone retiring_thread must still have been reclaimed exactly once
    ASSERT_ARE_EQUAL(int32_t, 1, interlocked_add(&g_reclaim_count, 0), "stranded node was never reclaimed (leak)");
}

// Regression for the live sweep path: a node handed off to the instance-wide pending reclaim list
// (because the thread that retired it unregistered while the node was still protected) must be
// reclaimed by the next reclaim cycle that runs on any other live thread, once the node is no longer
// protected - it must not have to wait for clds_hazard_pointers_destroy.
/* Tests_SRS_CLDS_HAZARD_POINTERS_42_002: [ When a reclaim cycle is triggered, it shall also reclaim each entry on the global pending reclaim list whose node is no longer protected by any hazard pointer and re-park the rest. ]*/
TEST_FUNCTION(pending_node_is_reclaimed_by_a_later_reclaim_cycle)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE retiring_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(retiring_thread);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE holding_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(holding_thread);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE sweeper_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(sweeper_thread);
    int* node = malloc(sizeof(int));
    ASSERT_IS_NOT_NULL(node);
    int* dummy = malloc(sizeof(int));
    ASSERT_IS_NOT_NULL(dummy);

    // holding_thread protects node with a hazard pointer so it cannot be reclaimed yet
    CLDS_HAZARD_POINTER_RECORD_HANDLE held_record = clds_hazard_pointers_acquire(holding_thread, node);
    ASSERT_IS_NOT_NULL(held_record);

    // act
    // retiring_thread retires node (stranded on its reclaim list because node is protected) then
    // unregisters, handing the entry off to the instance-wide pending reclaim list
    clds_hazard_pointers_reclaim(retiring_thread, node, test_reclaim);
    clds_hazard_pointers_unregister_thread(retiring_thread);
    ThreadAPI_Sleep(CLEANUP_WORKER_GRACE_MS);
    ASSERT_ARE_EQUAL(int32_t, 0, interlocked_add(&g_reclaim_count, 0), "node must not be reclaimed while a hazard pointer is held");

    // the holder releases so node is now unprotected, but no reclaim cycle has run yet
    clds_hazard_pointers_release(holding_thread, held_record);

    // a reclaim cycle on a different live thread must sweep the pending list and reclaim node
    clds_hazard_pointers_reclaim(sweeper_thread, dummy, test_reclaim);

    // assert
    // both the dummy (reclaimed directly by the sweeper) and the stranded node (swept from the
    // pending list during the sweeper's reclaim cycle, before any destroy) must have run
    ASSERT_ARE_EQUAL(int32_t, 2, interlocked_add(&g_reclaim_count, 0), "pending node was not swept by a later reclaim cycle");

    // cleanup
    clds_hazard_pointers_unregister_thread(holding_thread);
    clds_hazard_pointers_unregister_thread(sweeper_thread);
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
