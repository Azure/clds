// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>

#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/interlocked.h"
#include "c_pal/sync.h"

#include "c_pal/interlocked_hl.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/thandle.h"
#include "c_pal/job_object_helper.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hash_table.h"

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
    uint32_t appendix;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

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

static void test_skipped_seq_no_ignore(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    gballoc_hl_init(NULL, NULL);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

// 200 MB should be enough for the below test
#define DESIRED_USAGE 200 * 1024 * 1024

// This test is reproducing the issue of "doubling memory usage"
// The problem stems in the fact that we keep a counter that should indicate when the array oif hash table buckets should be doubled
// This counter was incorrectly incremented also when insert would return "already exists"
// 
// Test steps:
// 1. Create a hash table
// 2. Setup a job object that limits memory usage to a percentage that hovers around 200MB (note that the job object helper is somewhat innacurate because it uses %)
// 3. Insert an item
// 4. Insert the item again (triggers the problem)
// 5. Remove the item (rinse and repeat 3.-5. a million times)
TEST_FUNCTION(clds_hash_table_insert_same_block_again_does_not_leak_item_count)
{
    // arrange
    // 1. Create a hash table
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 500, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    int64_t insert_seq_no;
    CLDS_HASH_TABLE_ITEM* already_exists_item;

    uint64_t last_item = 0;

    // 2. Setup a job object that limits memory usage to a percentage that hovers around 200MB (note that the job object helper is somewhat innacurate because it uses %)
    THANDLE(JOB_OBJECT_HELPER) job_object_helper = job_object_helper_create();
    ASSERT_IS_NOT_NULL(job_object_helper);

    MEMORYSTATUSEX memory_status_ex;
    memory_status_ex.dwLength = sizeof(memory_status_ex);
    ASSERT_IS_TRUE(GlobalMemoryStatusEx(&memory_status_ex));

    // compute how much to set as limit in %
    uint8_t percent_to_use = (uint8_t)((uint64_t)DESIRED_USAGE * 100 / memory_status_ex.ullTotalPhys);
    if (percent_to_use == 0)
    {
        percent_to_use = 1;
    }

    ASSERT_ARE_EQUAL(int, 0, job_object_helper_limit_memory(job_object_helper, 1));

    already_exists_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(already_exists_item);

    // act
    for (size_t i = 0; i < 10000000; i++)
    {
        // 3. Insert an item
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        ASSERT_IS_NOT_NULL(item);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(0x42 + last_item), item, &insert_seq_no));

        // 4. Insert the item again (triggers the problem)
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS, clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(0x42 + last_item), already_exists_item, &insert_seq_no));

        // 5. Remove the item (rinse and repeat 3.-5. a million times)
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)(0x42 + last_item), NULL));
        last_item++;
    }

    // assert

    // act

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, already_exists_item);
    clds_hazard_pointers_destroy(hazard_pointers);

    THANDLE_ASSIGN(JOB_OBJECT_HELPER)(&job_object_helper, NULL);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
