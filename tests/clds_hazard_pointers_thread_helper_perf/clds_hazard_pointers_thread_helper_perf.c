// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>

#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/interlocked.h"
#include "c_pal/sync.h"

#include "c_pal/interlocked_hl.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/timer.h"
#include "c_pal/threadapi.h"

#include "c_util/thread_notifications_dispatcher.h"

#include "clds/clds_hazard_pointers_thread_helper.h"
#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hash_table.h"

TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    gballoc_hl_init(NULL, NULL);
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_dispatcher_init());
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    thread_notifications_dispatcher_deinit();
    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

// The item to be held in the hash table
typedef struct TEST_ITEM_TAG
{
    uint32_t key;
    uint32_t appendix;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

// Thread information (handle, start key, etc.)
typedef struct THREAD_DATA_TAG
{
    THREAD_HANDLE thread_handle;
    uint32_t key;
    struct CLDS_HP_PERF_TEST_CONTEXT_TAG* perf_test_context;
} THREAD_DATA;

// Test context: hash table handle, hazard pointers, etc.
typedef struct CLDS_HP_PERF_TEST_CONTEXT_TAG
{
    volatile_atomic int64_t sequence_number;
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE clds_hazard_pointers_thread_helper;
    volatile_atomic int32_t used_threads_head;
    volatile_atomic int32_t stop_requested;
    THREAD_DATA* thread_data;
    volatile_atomic int64_t total_inserts;
    volatile_atomic int32_t active_thread_count;
} CLDS_HP_PERF_TEST_CONTEXT;

// How much should the test run?
#define TEST_RUNTIME                (60000) // ms

// How much should each thread run before exiting?
#define TEST_THREAD_RUNTIME         (20) // ms

// How many insert/deletes should be done before checking if the thread should exit
// It's sort of a quanta
#define TEST_THREAD_EXECUTION_BATCH 200

// How many threads should be doing operations concurrently
#define TEST_DESIRED_THREAD_COUNT   1

static int insert_and_delete_thread_func(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i;
    double start_time = timer_global_get_elapsed_ms();
    double current_time;

    CLDS_HP_PERF_TEST_CONTEXT* perf_test_context = thread_data->perf_test_context;

    // get the hazard pointers thread handle
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(perf_test_context->clds_hazard_pointers_thread_helper);
    ASSERT_IS_NOT_NULL(clds_hazard_pointers_thread);

    do
    {
        // perform a number of insert and delete actions
        for (i = 0; i < TEST_THREAD_EXECUTION_BATCH; i++)
        {
            CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
            TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
            test_item->key = i;
            int64_t insert_seq_no;

            ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK,
                clds_hash_table_insert(perf_test_context->hash_table, clds_hazard_pointers_thread, (void*)(uintptr_t)(thread_data->key + i + 1), item, &insert_seq_no));

            // delete it right away
            ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, 
                clds_hash_table_delete(perf_test_context->hash_table, clds_hazard_pointers_thread, (void*)(uintptr_t)(thread_data->key + i + 1), &insert_seq_no));

            (void)interlocked_increment_64(&perf_test_context->total_inserts);
        }

        // check if the thread is done
        current_time = timer_global_get_elapsed_ms();
    } while (current_time - start_time < TEST_THREAD_RUNTIME);

    // signal that the thread is done with its work
    (void)InterlockedHL_DecrementAndWake(&perf_test_context->active_thread_count);

    result = 0;

    return result;
}

static uint64_t test_compute_hash(void* key)
{
    return (uint64_t)key;
}

static int test_key_compare(void* key1, void* key2)
{
    int result;

    if ((int64_t)key1 < (int64_t)key2)
    {
        result = -1;
    }
    else if ((int64_t)key1 > (int64_t)key2)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

// This is the ma number of threads that we can have in the test
// if we get to that many threads we have other issues in the test
#define MAX_TEST_THREADS 100000

// How long should we wait for the number of active threads to change
#define STOP_POLL_INTERVAL (100) // ms

// This is a special thread that is a manager
// It spawns new threads that do the work as needed in order to maintain a certain number of threads
// that are active (are doing work)
static int thread_spawner_thread_func(void* arg)
{
    CLDS_HP_PERF_TEST_CONTEXT* perf_test_context = arg;

    // while the test is still running
    while (!interlocked_add(&perf_test_context->stop_requested, 0))
    {
        // check if we have to spawn a new thread
        int32_t current_active_thread_count = interlocked_add(&perf_test_context->active_thread_count, 0);
        if (current_active_thread_count < TEST_DESIRED_THREAD_COUNT)
        {
            // get the entry in the array where we should spawn it
            int32_t current_used_threads_head = interlocked_add(&perf_test_context->used_threads_head, 0);
            ASSERT_IS_TRUE(current_used_threads_head < MAX_TEST_THREADS, "Exceeded max test threads");

            THREAD_DATA* thread_data = &perf_test_context->thread_data[current_used_threads_head];

            thread_data->key = perf_test_context->used_threads_head * 1000000;
            thread_data->perf_test_context = perf_test_context;

            // start the thread
            ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&perf_test_context->thread_data[current_used_threads_head].thread_handle, insert_and_delete_thread_func, thread_data));
            (void)interlocked_increment(&perf_test_context->used_threads_head);

            (void)interlocked_increment(&perf_test_context->active_thread_count);
        }
        else
        {
            // we want to check stop_requested and active_thread_count, so we'd need some sort of wait for multiple values, which we do not have
            // thus the next best thing is to wait for a while for active_thread_count
            // but no-one wants to spurious logs from InterlockedHL_WaitForNotValue that the time elapsed - it makes the logs horrible to read, thus we'll go with the lower level APIs
            // that is since we don't really care about spurious wakes
            (void)wait_on_address(&perf_test_context->active_thread_count, current_active_thread_count, STOP_POLL_INTERVAL);
        }
    }

    // wait for all active threads to die down
    ASSERT_ARE_EQUAL(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_OK, InterlockedHL_WaitForValue(&perf_test_context->active_thread_count, 0, UINT32_MAX));

    return 0;
}

static void test_skipped_seq_chaos(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

TEST_FUNCTION(clds_hash_table_perf_does_not_degrade_over_time_with_more_and_more_threads_being_used)
{
    // The test wants to constantly have new threads do the work, while old threads
    // are forgotten and let die.
    // Thus threads are spun up, do work for a certain amount of time and then they exit
    // The test has a manager thread that makes sure that new threads are spun up to do more work
    // 
    // The test measures the number of operations that are performed in 1s slices
    // The expectation is that this number of operations per second does not degrade over time
    // 
    // arrange
    CLDS_HP_PERF_TEST_CONTEXT perf_test_context;
    perf_test_context.hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(perf_test_context.hazard_pointers);

    (void)interlocked_exchange_64(&perf_test_context.sequence_number, 0);

    // create a hash table
    perf_test_context.hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 8, perf_test_context.hazard_pointers, &perf_test_context.sequence_number, test_skipped_seq_chaos, NULL);
    ASSERT_IS_NOT_NULL(perf_test_context.hash_table);

    // create the helper for hazard pointers
    perf_test_context.clds_hazard_pointers_thread_helper = clds_hazard_pointers_thread_helper_create(perf_test_context.hazard_pointers);
    ASSERT_IS_NOT_NULL(perf_test_context.clds_hazard_pointers_thread_helper);

    // initialize number of active threads to 0 and the rest of variables
    (void)interlocked_exchange(&perf_test_context.active_thread_count, 0);
    (void)interlocked_exchange(&perf_test_context.stop_requested, 0);
    (void)interlocked_exchange_64(&perf_test_context.total_inserts, 0);
    (void)interlocked_exchange(&perf_test_context.used_threads_head, 0);

    // allocate an array of thread structures where we keep the handle for each thread and other
    // thread relevant information
    perf_test_context.thread_data = malloc_2(MAX_TEST_THREADS, sizeof(THREAD_DATA));
    ASSERT_IS_NOT_NULL(perf_test_context.thread_data);

    // start the manager thread
    THREAD_HANDLE thread_spawner_thread;
    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&thread_spawner_thread, thread_spawner_thread_func, &perf_test_context));

    // wait and print the number of inserts done
    double start_time = timer_global_get_elapsed_ms();
    double current_time;
    int64_t inserts_first_second = 0;
    do
    {
        int64_t last_insert_count = interlocked_add_64(&perf_test_context.total_inserts, 0);

        ThreadAPI_Sleep(1000);

        current_time = timer_global_get_elapsed_ms();

        int64_t current_insert_count = interlocked_add_64(&perf_test_context.total_inserts, 0);
        int32_t threads_spawned = interlocked_add(&perf_test_context.used_threads_head, 0);
        LogInfo("Elapsed: %.02f, total inserts=%" PRIu64 ", last second inserts=%" PRIu64 ", total threads spawned=%" PRIu32 "",
            current_time - start_time, current_insert_count, current_insert_count - last_insert_count, threads_spawned);

        if (inserts_first_second == 0)
        {
            inserts_first_second = current_insert_count - last_insert_count;
        }
        else
        {
            ASSERT_IS_TRUE((current_insert_count - last_insert_count >= inserts_first_second / 2), "Expected inserts/s to not degrade, but it did, first iteration=%" PRIu64 ", current iteration=%" PRIu64 "", inserts_first_second, current_insert_count - last_insert_count);
        }
    } while (current_time - start_time < TEST_RUNTIME);

    // stop the test
    (void)interlocked_exchange(&perf_test_context.stop_requested, 1);
    wake_by_address_all(&perf_test_context.stop_requested);

    // assert
    int dont_care;
    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(thread_spawner_thread, &dont_care));

    // join all threads to make sure we exit cleanly

    int32_t used_threads_head = interlocked_add(&perf_test_context.used_threads_head, 0);
    for (int32_t i = 0; i < used_threads_head; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(perf_test_context.thread_data[i].thread_handle, &dont_care));
    }

    // cleanup
    free(perf_test_context.thread_data);
    clds_hazard_pointers_thread_helper_destroy(perf_test_context.clds_hazard_pointers_thread_helper);
    clds_hash_table_destroy(perf_test_context.hash_table);
    clds_hazard_pointers_destroy(perf_test_context.hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
