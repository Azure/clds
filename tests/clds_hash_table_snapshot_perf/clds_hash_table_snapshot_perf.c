// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>

#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/timer.h"

#include "c_util/thread_notifications_dispatcher.h"

#include "clds/clds_hazard_pointers_thread_helper.h"
#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hash_table.h"

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);

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
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

#define TEST_ITEM_COUNT                (500000)

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

TEST_FUNCTION(clds_hash_table_snapshot_perf_is_decent)
{
    // The test wants to test the snapshot of a hash table that contains many items
    // arrange
    volatile_atomic int64_t sequence_number;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;

    clds_hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);

    clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(clds_hazard_pointers_thread);

    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1024 * 1024, clds_hazard_pointers, &sequence_number, NULL, NULL);
    ASSERT_IS_NOT_NULL(hash_table);

    // insert some items
    for (uint32_t i = 0; i < TEST_ITEM_COUNT; i++)
    {
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        ASSERT_IS_NOT_NULL(item);
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);

        test_item->key = i + 1;

        int64_t insert_sequence_number;
        CLDS_HASH_TABLE_INSERT_RESULT result = clds_hash_table_insert(hash_table, clds_hazard_pointers_thread, (void*)(uintptr_t)test_item->key, (void*)item, &insert_sequence_number);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

        if (i % 1000 == 0)
        {
            LogInfo("Inserted %" PRIu32 " items", i);
        }

        // ugh, this is very old and has transfer of ownership on insert, grrr
        // so no release here
    }

    // act
    double start_time = timer_global_get_elapsed_ms();
    CLDS_HASH_TABLE_ITEM** items = NULL;
    uint64_t item_count;
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, clds_hash_table_snapshot(hash_table, clds_hazard_pointers_thread, &items, &item_count));
    double end_time = timer_global_get_elapsed_ms();

    LogInfo("Snapshot took %.02f ms", end_time - start_time);

    // assert
    ASSERT_IS_TRUE(end_time - start_time < 1000, "Snapshot took %.02f ms", end_time - start_time);

    // cleanup
    for (uint64_t i = 0; i < item_count; i++)
    {
        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, items[i]);
    }
    free(items);
    clds_hazard_pointers_unregister_thread(clds_hazard_pointers_thread);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
