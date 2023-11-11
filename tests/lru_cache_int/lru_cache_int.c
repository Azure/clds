// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/interlocked.h"
#include "c_pal/interlocked_hl.h"
#include "c_pal/timer.h"
#include "c_pal/thandle.h"
#include "c_pal/threadapi.h"
#include "c_pal/sync.h"
#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hash_table.h"
#include "c_util/thread_notifications_dispatcher.h"

#include "clds/lru_cache.h"

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
    uint32_t appendix;
    uint32_t item_size;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

typedef struct CHAOS_TEST_ITEM_DATA_TAG
{
    CLDS_HASH_TABLE_ITEM* item;
    volatile_atomic int32_t item_state;
} CHAOS_TEST_ITEM_DATA;

typedef struct CHAOS_TEST_CONTEXT_TAG
{
    volatile_atomic int32_t done;
    volatile_atomic int32_t eviction_call_count;
    volatile_atomic int32_t put_call_count;
    LRU_CACHE_HANDLE lru_cache;
    CHAOS_TEST_ITEM_DATA items[];
} CHAOS_TEST_CONTEXT;

typedef struct CHAOS_THREAD_DATA_TAG
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    THREAD_HANDLE thread_handle;
    CHAOS_TEST_CONTEXT* chaos_test_context;
} CHAOS_THREAD_DATA;

#define CHAOS_THREAD_COUNT  16
#define CHAOS_ITEM_COUNT    5000
#define CHAOS_MAX_CAPACITY   30000

#ifdef USE_VALGRIND
// when using Valgrind/Helgrind setting this way lower as that is ... slow
#define CHAOS_TEST_RUNTIME  10000 // ms
#else
// Otherwise run for longer
#define CHAOS_TEST_RUNTIME  300000 // ms
#endif

#define TEST_HASH_TABLE_ITEM_STATE_VALUES \
    TEST_HASH_TABLE_ITEM_NOT_USED, \
    TEST_HASH_TABLE_ITEM_INSERTING, \
    TEST_HASH_TABLE_ITEM_USED, \
    TEST_HASH_TABLE_ITEM_EVICTED, \
    TEST_HASH_TABLE_ITEM_EVICTING, \
    TEST_HASH_TABLE_ITEM_FINDING_NOT_FOUND

MU_DEFINE_ENUM(TEST_HASH_TABLE_ITEM_STATE, TEST_HASH_TABLE_ITEM_STATE_VALUES);
MU_DEFINE_ENUM_STRINGS(TEST_HASH_TABLE_ITEM_STATE, TEST_HASH_TABLE_ITEM_STATE_VALUES)


#define CHAOS_TEST_ACTION_VALUES \
    CHAOS_TEST_ACTION_PUT, \
    CHAOS_TEST_ACTION_PUT_EVICTED, \
    CHAOS_TEST_ACTION_GET_NOT_FOUND, \
    CHAOS_TEST_ACTION_GET_EVICTED, \
    CHAOS_TEST_ACTION_GET

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);
MU_DEFINE_ENUM_STRINGS(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES)

TEST_DEFINE_ENUM_TYPE(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT_VALUES);


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
    ASSERT_ARE_EQUAL(int, 0, gballoc_hl_init(NULL, NULL));
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_dispatcher_init());
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    thread_notifications_dispatcher_deinit();
}

typedef struct EVICT_CONTEXT_TAG
{
    uint32_t count;
} EVICT_CONTEXT;

static void on_evict_callback(void* context, void* item)
{
    EVICT_CONTEXT* count_context = context;
    count_context->count++;
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    ASSERT_IS_NOT_NULL(test_item);
}


static void on_lru_cache_error_callback(void* context)
{
    (void)context;
    ASSERT_FAIL("LRU_CACHE error");
}

typedef struct EVICTION_TEST_CONTEXT_TAG
{
    uint32_t key;
    volatile_atomic int32_t was_called;
} EVICTION_TEST_CONTEXT;


static void test_success_eviction(void* context, void* evicted_value)
{
    EVICTION_TEST_CONTEXT* evict_context = context;
    ASSERT_IS_NOT_NULL(evict_context);
    ASSERT_IS_NOT_NULL(evicted_value);

    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, evicted_value);
    ASSERT_IS_NOT_NULL(test_item);

    ASSERT_ARE_EQUAL(uint32_t, evict_context->key, test_item->key);

    (void)interlocked_increment(&evict_context->was_called);
    wake_by_address_single(&evict_context->was_called);
}

TEST_FUNCTION(test_put_and_get)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, 3, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);


    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item2);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);


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
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, 2, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);


    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    CLDS_HASH_TABLE_ITEM* item3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item3);
    TEST_ITEM* test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item3);
    test_item3->key = 3;
    test_item3->appendix = 15;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item3, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);


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


TEST_FUNCTION(test_put_calls_evict_with_correct_context)
{
    // arrange
    uint32_t key1 = 1, key2 = 2, key3 = 3;
    int64_t capacity = 2;
    EVICTION_TEST_CONTEXT key1_context, key2_context, key3_context;
    key1_context.key = key1, key2_context.key = key2, key3_context.key = key3;
    (void)interlocked_exchange(&key1_context.was_called, 0);
    (void)interlocked_exchange(&key2_context.was_called, 0);
    (void)interlocked_exchange(&key3_context.was_called, 0);


    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, capacity, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);


    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = key1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = key2;
    test_item2->appendix = 14;

    CLDS_HASH_TABLE_ITEM* item3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item3);
    TEST_ITEM* test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item3);
    test_item3->key = key3;
    test_item3->appendix = 15;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, test_success_eviction, &key1_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, test_success_eviction, &key2_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item3, 2, test_success_eviction, &key3_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);


    // assert
    ASSERT_ARE_EQUAL(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_OK, InterlockedHL_WaitForValue(&key1_context.was_called, 1, UINT32_MAX));
    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NULL(return_val1);

    ASSERT_ARE_EQUAL(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_OK, InterlockedHL_WaitForValue(&key2_context.was_called, 1, UINT32_MAX));
    CLDS_HASH_TABLE_ITEM* return_val2 = lru_cache_get(lru_cache, (void*)(uintptr_t)(2));
    ASSERT_IS_NULL(return_val2);

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
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, 2, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);


    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item2);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    CLDS_HASH_TABLE_ITEM* item3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item3);
    TEST_ITEM* test_item3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item3);
    test_item3->key = 3;
    test_item3->appendix = 15;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    // act
    for (int i = 0; i < call_times; i++)
    {
        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(3), item3, 1, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    }

    // assert
    ASSERT_ARE_EQUAL(uint32_t, call_times * 3, count_context.count);

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
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, 3, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);


    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item2);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    // Lets fill the capacity
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(2), item2, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    // act
    for (int i = 0; i < call_times; i++)
    {
        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
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
    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, 3, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);

    LRU_CACHE_PUT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item1);
    TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item1);
    test_item1->key = 1;
    test_item1->appendix = 13;

    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(item2);
    TEST_ITEM* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item2);
    test_item2->key = 2;
    test_item2->appendix = 14;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item1, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    CLDS_HASH_TABLE_ITEM* return_val1 = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val1);
    TEST_ITEM* return_test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val1);
    ASSERT_IS_NOT_NULL(return_test_item1);
    ASSERT_ARE_EQUAL(int, 1, return_test_item1->key);
    ASSERT_ARE_EQUAL(int, 13, return_test_item1->appendix);

    // act
    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), item2, 1, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);


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


TEST_FUNCTION(test_put_same_key_calls_evict_to_make_space)
{
    // arrange
    EVICT_CONTEXT count_context = { 0 };

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    uint64_t capacity = 11;
    int n = 10;
    CLDS_HASH_TABLE_ITEM** items = malloc_2(n, sizeof(CLDS_HASH_TABLE_ITEM*));

    LRU_CACHE_HANDLE lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, capacity, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(lru_cache);

    LRU_CACHE_PUT_RESULT result;

    for (int i = 0; i < n; i++)
    {
        items[i] = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        ASSERT_IS_NOT_NULL(items[i]);
        TEST_ITEM* test_item1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, items[i]);
        test_item1->key = i + 1;
        test_item1->appendix = i + 10;

        result = lru_cache_put(lru_cache, (void*)(uintptr_t)(i + 1), items[i], 1, on_evict_callback, &count_context);
        ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    }

    CLDS_HASH_TABLE_ITEM* large_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    ASSERT_IS_NOT_NULL(large_item);
    TEST_ITEM* test_large_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, large_item);
    test_large_item->key = 100013;
    test_large_item->appendix = 110013;

    result = lru_cache_put(lru_cache, (void*)(uintptr_t)(1), large_item, 11, on_evict_callback, &count_context);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    // assert
    ASSERT_ARE_EQUAL(uint32_t, 9, count_context.count);

    CLDS_HASH_TABLE_ITEM* return_val = lru_cache_get(lru_cache, (void*)(uintptr_t)(1));
    ASSERT_IS_NOT_NULL(return_val);
    TEST_ITEM* return_test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val);
    ASSERT_IS_NOT_NULL(return_test_item);
    ASSERT_ARE_EQUAL(int, 100013, return_test_item->key);
    ASSERT_ARE_EQUAL(int, 110013, return_test_item->appendix);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, large_item);
    for (int i = 0; i < n; i++)
    {
        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, items[i]);
    }
    free(items);
    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static void on_chaos_evict_callback(void* context, void* item)
{
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    ASSERT_IS_NOT_NULL(test_item);
    CHAOS_TEST_CONTEXT* evict_context = context;
    CHAOS_TEST_ITEM_DATA* data = &evict_context->items[test_item->key - 1];
    (void)interlocked_exchange(&data->item_state, TEST_HASH_TABLE_ITEM_EVICTING);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, data->item);
    data->item = NULL;
    (void)interlocked_exchange(&data->item_state, TEST_HASH_TABLE_ITEM_EVICTED);
    (void)interlocked_increment(&evict_context->eviction_call_count);
}

static bool get_item_and_change_state(CHAOS_TEST_ITEM_DATA* items, int item_count, int32_t new_item_state, int32_t old_item_state, int* selected_item_index)
{
    int item_index = (rand() * (item_count - 1)) / RAND_MAX;
    int start_item_index = item_index;
    bool result;

    do
    {
        if (interlocked_compare_exchange(&items[item_index].item_state, new_item_state, old_item_state) == old_item_state)
        {
            *selected_item_index = item_index;
            result = true;
            break;
        }
        else
        {
            item_index++;
            if (item_index == item_count)
            {
                item_index = 0;
            }

            if (item_index == start_item_index)
            {
                result = false;
                break;
            }
        }
    } while (1);

    return result;
}

static int chaos_thread(void* arg)
{
    int result;
    CHAOS_THREAD_DATA* chaos_thread_data = arg;
    CHAOS_TEST_CONTEXT* chaos_test_context = chaos_thread_data->chaos_test_context;

    srand((unsigned int)time(NULL));

    while (interlocked_add(&chaos_test_context->done, 0) != 1)
    {
        // perform one of the several actions
        CHAOS_TEST_ACTION action = (CHAOS_TEST_ACTION)(rand() * ((MU_COUNT_ARG(CHAOS_TEST_ACTION_VALUES)) - 1) / RAND_MAX);
        int item_index = (rand() * (CHAOS_ITEM_COUNT - 1)) / RAND_MAX;
        int item_size = ((rand() * (CHAOS_MAX_CAPACITY/100)) / RAND_MAX) + 1;
        //int item_size = 10;

        switch (action)
        {
            default:
                LogError("Invalid action: %d", action);
                break;

            case CHAOS_TEST_ACTION_GET:
            {
                if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_USED, TEST_HASH_TABLE_ITEM_USED, &item_index))
                {
                    if (chaos_test_context->items[item_index].item != NULL)
                    {
                        TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                        item_payload->key = item_index + 1;
                        item_payload->appendix = item_index + 100;

                        CLDS_HASH_TABLE_ITEM* return_val = lru_cache_get(chaos_test_context->lru_cache, (void*)(uintptr_t)(item_index + 1));
                        // At this point the item could have been evicted or one of the thread coulds have changed the state to something else.
                        if (return_val != NULL)
                        {
                            ASSERT_IS_NOT_NULL(return_val, "Key=%d, state is %" PRI_MU_ENUM "", item_index + 1, MU_ENUM_VALUE(TEST_HASH_TABLE_ITEM_STATE, interlocked_add(&chaos_test_context->items[item_index].item_state, 0)));
                            TEST_ITEM* return_test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, return_val);
                            ASSERT_IS_NOT_NULL(return_test_item);
                            ASSERT_ARE_EQUAL(int, item_index + 1, return_test_item->key);
                            ASSERT_ARE_EQUAL(int, item_index + 100, return_test_item->appendix);
                        }
                    }
                }
                break;
            }
            case CHAOS_TEST_ACTION_GET_EVICTED:
            {
                if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING_NOT_FOUND, TEST_HASH_TABLE_ITEM_EVICTED, &item_index))
                {
                    CLDS_HASH_TABLE_ITEM* return_val = lru_cache_get(chaos_test_context->lru_cache, (void*)(uintptr_t)(item_index + 1));
                    ASSERT_IS_NULL(return_val, "Key=%d", item_index + 1);
                    (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
                }
                break;
            }
            case CHAOS_TEST_ACTION_GET_NOT_FOUND:
            {
                if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING_NOT_FOUND, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
                {
                    CLDS_HASH_TABLE_ITEM* return_val = lru_cache_get(chaos_test_context->lru_cache, (void*)(uintptr_t)(item_index + 1));
                    ASSERT_IS_NULL(return_val, "Key=%d", item_index + 1);
                    (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
                }
                break;
            }
            case CHAOS_TEST_ACTION_PUT:
            {
                if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
                {
                    chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                    ASSERT_IS_NOT_NULL(chaos_test_context->items[item_index].item);
                    TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                    item_payload->key = item_index + 1;
                    item_payload->appendix = item_index + 100;
                    item_payload->item_size = item_size;

                    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, lru_cache_put(chaos_test_context->lru_cache, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, item_size, on_chaos_evict_callback, chaos_test_context));
                    (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
                    (void)interlocked_increment(&chaos_test_context->put_call_count);
                }
                break;
            }
            case CHAOS_TEST_ACTION_PUT_EVICTED:
            {
                if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING, TEST_HASH_TABLE_ITEM_EVICTED, &item_index))
                {
                    chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                    ASSERT_IS_NOT_NULL(chaos_test_context->items[item_index].item);
                    TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                    item_payload->key = item_index + 1;
                    item_payload->appendix = item_index + 100;
                    item_payload->item_size = item_size;

                    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, lru_cache_put(chaos_test_context->lru_cache, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, item_size, on_chaos_evict_callback, chaos_test_context));
                    (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
                    (void)interlocked_increment(&chaos_test_context->put_call_count);
                }
                break;
            }
        }
    }
    result = 0;
    return result;
}


TEST_FUNCTION(lru_cache_chaos_knight_test)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    size_t i;


    CHAOS_TEST_CONTEXT* chaos_test_context = malloc_flex(sizeof(CHAOS_TEST_CONTEXT), CHAOS_ITEM_COUNT, sizeof(CHAOS_TEST_ITEM_DATA));
    ASSERT_IS_NOT_NULL(chaos_test_context);

    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        chaos_test_context->items[i].item = NULL;
        (void)interlocked_exchange(&chaos_test_context->items[i].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
    }

    chaos_test_context->lru_cache = lru_cache_create(test_compute_hash, test_key_compare, 1, hazard_pointers, CHAOS_MAX_CAPACITY, on_lru_cache_error_callback, NULL);
    ASSERT_IS_NOT_NULL(chaos_test_context->lru_cache);

    (void)interlocked_exchange(&chaos_test_context->done, 0);
    (void)interlocked_exchange(&chaos_test_context->eviction_call_count, 0);
    (void)interlocked_exchange(&chaos_test_context->put_call_count, 0);

    // start threads doing random things on the list
    CHAOS_THREAD_DATA* chaos_thread_data = malloc_2(CHAOS_THREAD_COUNT, sizeof(CHAOS_THREAD_DATA));
    ASSERT_IS_NOT_NULL(chaos_thread_data);

    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        chaos_thread_data[i].chaos_test_context = chaos_test_context;
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&chaos_thread_data[i].thread_handle, chaos_thread, &chaos_thread_data[i]), "Error spawning test thread %zu", i);
    }

    double start_time = timer_global_get_elapsed_ms();

    LogInfo("Running chaos test for %.02f seconds", (double)CHAOS_TEST_RUNTIME / 1000);

    // act
    while (timer_global_get_elapsed_ms() - start_time < CHAOS_TEST_RUNTIME)
    {
        LogInfo("Test ran for %.02f seconds", (timer_global_get_elapsed_ms() - start_time) / 1000);
        ThreadAPI_Sleep(1000);
    }

    (void)interlocked_exchange(&chaos_test_context->done, 1);

    int dont_care;
    // assert
    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(chaos_thread_data[i].thread_handle, &dont_care), "Thread %zu failed to join", i);
    }

    int32_t count = 0;
    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        if (chaos_test_context->items[i].item != NULL)
        {
            //LogInfo("The state(%" PRI_MU_ENUM ") KEY = %zu", MU_ENUM_VALUE(TEST_HASH_TABLE_ITEM_STATE, chaos_test_context->items[i].item_state), i + 1);           
            count++;
            CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, chaos_test_context->items[i].item);
        }
    }
    LogInfo("Count: %d", count);
    LogInfo("Eviction calls count: %d", chaos_test_context->eviction_call_count);
    LogInfo("Put calls count: %d", chaos_test_context->put_call_count);
    // This ensures we did right amount of evictions
    ASSERT_ARE_EQUAL(int32_t, chaos_test_context->eviction_call_count + count, chaos_test_context->put_call_count);

    // cleanup
    free(chaos_thread_data);
    lru_cache_destroy(chaos_test_context->lru_cache);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
