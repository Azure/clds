// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>


#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_pal/interlocked.h"
#include "c_pal/sync.h"
#include "c_pal/uuid.h"

#include "c_pal/interlocked_hl.h"

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/timer.h"
#include "c_pal/threadapi.h"

#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hash_table.h"

#include "clds/lru_cache.h"

#define XTEST_FUNCTION(A) void A(void)

#define SEQ_NO_STATE_VALUES \
    SEQ_NO_NOT_USED, \
    SEQ_NO_USED

MU_DEFINE_ENUM(SEQ_NO_STATE, SEQ_NO_STATE_VALUES);

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(SEQ_NO_STATE, SEQ_NO_STATE_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);

#define THREAD_COUNT 4

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

static uint64_t test_compute_hash_modulo_4(void* key)
{
    return (uint64_t)key % 4;
}

static void test_skipped_seq_no_ignore(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

static void test_item_cleanup_func(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item)
{
    (void)context;
    (void)item;
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


TEST_FUNCTION(cyrus)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    //CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    //ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 4);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    test_item->key = 1;
    test_item->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 2;
    test_item1->appendix = 14;

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item, 1);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item1, 1);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item1, 1);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item, 1);
    ASSERT_ARE_EQUAL(int, 0, result);


    CLDS_HASH_TABLE_ITEM* return_val = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    TEST_ITEM* return_val1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val);

    if (return_val1)
    {

    }
    // cleanup
    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}


END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
