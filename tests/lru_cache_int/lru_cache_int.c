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

typedef struct EVICT_CONTEXT_TAG
{
    uint32_t count;
} EVICT_CONTEXT;

void on_evict_callback(void* context, LRU_CACHE_EVICT_RESULT cache_evict_status)
{
    EVICT_CONTEXT* count_context = context;
    count_context->count++;
    (void)cache_evict_status;
}

TEST_FUNCTION(test_put_and_get)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 3);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);


    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    TEST_ITEM* return_test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val1);
    ASSERT_IS_NOT_NULL(return_test_item);
    ASSERT_ARE_EQUAL(int, 1, return_test_item->key);
    ASSERT_ARE_EQUAL(int, 13, return_test_item->appendix);

    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(2));
    TEST_ITEM* return_test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val2);
    ASSERT_IS_NOT_NULL(return_test_item2);
    ASSERT_ARE_EQUAL(int, 2, return_test_item2->key);
    ASSERT_ARE_EQUAL(int, 14, return_test_item2->appendix);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item1);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item2);

    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}


TEST_FUNCTION(test_put_calls_evict)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 3);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    CLDS_HASH_TABLE_ITEM* item3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item3);
    test_item3->key = 3;
    test_item3->appendix = 15;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item3, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);


    // assert
    ASSERT_ARE_EQUAL(uint32_t, 1, count_context.count);
    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NULL(return_val1);

    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(2));
    ASSERT_IS_NOT_NULL(return_val2);
    TEST_ITEM* return_test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val2);
    ASSERT_IS_NOT_NULL(return_test_item2);
    ASSERT_ARE_EQUAL(int, 2, return_test_item2->key);
    ASSERT_ARE_EQUAL(int, 14, return_test_item2->appendix);

    CLDS_HASH_TABLE_ITEM* return_val3 = lru_cache_get(lru_cache, (void*)(uintptr_t)(3));
    ASSERT_IS_NOT_NULL(return_val3);
    TEST_ITEM* return_test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val3);
    ASSERT_IS_NOT_NULL(return_test_item3);
    ASSERT_ARE_EQUAL(int, 3, return_test_item3->key);
    ASSERT_ARE_EQUAL(int, 15, return_test_item3->appendix);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item1);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item2);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item3);

    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(test_put_same_item_multiple_times_triggers_evict)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };
    int call_times = 10;

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 3);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    CLDS_HASH_TABLE_ITEM* item3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item3);
    test_item3->key = 3;
    test_item3->appendix = 15;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    // act
    for (int i = 0; i < call_times; i++)
    {
        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item3, 1, &sequence_number, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(int, 0, result);

        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(int, 0, result);

        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, &sequence_number, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(int, 0, result);
    }

    // assert
    ASSERT_ARE_EQUAL(uint32_t, call_times*3, count_context.count);

    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val1);
    TEST_ITEM* return_test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val1);
    ASSERT_IS_NOT_NULL(return_test_item1);
    ASSERT_ARE_EQUAL(int, 1, return_test_item1->key);
    ASSERT_ARE_EQUAL(int, 13, return_test_item1->appendix);

    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(2));
    ASSERT_IS_NOT_NULL(return_val2);
    TEST_ITEM* return_test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val2);
    ASSERT_IS_NOT_NULL(return_test_item2);
    ASSERT_ARE_EQUAL(int, 2, return_test_item2->key);
    ASSERT_ARE_EQUAL(int, 14, return_test_item2->appendix);

    CLDS_HASH_TABLE_ITEM* return_val3 = lru_cache_get(lru_cache, (void*)(uintptr_t)(3));
    ASSERT_IS_NULL(return_val3);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item1);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item2);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item3);

    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(test_put_multiple_times_on_same_item_does_not_trigger_evict)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };
    int call_times = 10;

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 3);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    // Lets fill the capacity
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    // act
    for (int i = 0; i < call_times; i++)
    {
        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(int, 0, result);
    }


    // assert
    ASSERT_ARE_EQUAL(uint32_t, 0, count_context.count);

    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val1);
    TEST_ITEM* return_test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val1);
    ASSERT_IS_NOT_NULL(return_test_item1);
    ASSERT_ARE_EQUAL(int, 1, return_test_item1->key);
    ASSERT_ARE_EQUAL(int, 13, return_test_item1->appendix);

    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(2));
    ASSERT_IS_NOT_NULL(return_val2);
    TEST_ITEM* return_test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val2);
    ASSERT_IS_NOT_NULL(return_test_item2);
    ASSERT_ARE_EQUAL(int, 2, return_test_item2->key);
    ASSERT_ARE_EQUAL(int, 14, return_test_item2->appendix);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item1);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item2);

    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}


TEST_FUNCTION(test_put_different_value_same_key_works)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    int64_t sequence_number = 45;
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556, 3);
    ASSERT_IS_NOT_NULL(lru_cache);


    int result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);

    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val1);
    TEST_ITEM* return_test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val1);
    ASSERT_IS_NOT_NULL(return_test_item1);
    ASSERT_ARE_EQUAL(int, 1, return_test_item1->key);
    ASSERT_ARE_EQUAL(int, 13, return_test_item1->appendix);

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item2, 1, &sequence_number, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(int, 0, result);


    // assert
    ASSERT_ARE_EQUAL(uint32_t, 0, count_context.count);

    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val2);
    TEST_ITEM* return_test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val2);
    ASSERT_IS_NOT_NULL(return_test_item2);
    ASSERT_ARE_EQUAL(int, 2, return_test_item2->key);
    ASSERT_ARE_EQUAL(int, 14, return_test_item2->appendix);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item1);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item2);

    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
