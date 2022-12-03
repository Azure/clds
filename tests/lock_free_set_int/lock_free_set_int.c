// Copyright (c) Microsoft. All rights reserved.

#include <stdbool.h>
#include <stdlib.h>

#include "testrunnerswitcher.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/threadapi.h"

#include "clds/lock_free_set.h"

#define THREAD_COUNT 4

#ifdef _MSC_VER
// on Windows run with more iterations. Normally this should be passed as an argument, but this should also do for now.
#define INSERT_COUNT 1000000
#else
// setting this way lower as we run with Helgrind on Linux and that is ... slow
#define INSERT_COUNT 10000
#endif

static TEST_MUTEX_HANDLE test_serialize_mutex;

typedef struct TEST_SET_ITEM_TAG
{
    LOCK_FREE_SET_ITEM item;
} TEST_SET_ITEM;

typedef struct THREAD_DATA_TAG
{
    LOCK_FREE_SET_HANDLE set;
    TEST_SET_ITEM* items[INSERT_COUNT];
} THREAD_DATA;

static int insert_and_remove_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = arg;
    int result;

    for (i = 0; i < INSERT_COUNT; i++)
    {
        result = lock_free_set_insert(thread_data->set, &thread_data->items[i]->item);
        ASSERT_ARE_EQUAL(int, 0, result, "lock_free_set_insert failed");

        result = lock_free_set_remove(thread_data->set, &thread_data->items[i]->item);
        ASSERT_ARE_EQUAL(int, 0, result, "lock_free_set_remove failed");
    }

    return result;
}

TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, gballoc_hl_init(NULL, NULL));

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    TEST_MUTEX_DESTROY(test_serialize_mutex);

    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(test_serialize_mutex))
    {
        ASSERT_FAIL("Could not acquire test serialization mutex.");
    }
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(test_serialize_mutex);
}

/* Tests_SRS_LOCK_FREE_SET_01_013: [ lock_free_set_insert and lock_free_set_remove shall be safe to be called from multiple threads. ]*/
TEST_FUNCTION(lock_free_set_insert_remove_succeeds)
{
    size_t i;
    LOCK_FREE_SET_HANDLE set;
    THREAD_HANDLE threads[THREAD_COUNT];
    THREAD_DATA* thread_data;

    // arrange
    set = lock_free_set_create();
    ASSERT_IS_NOT_NULL(set, "set creation failed");

    thread_data = malloc_2(THREAD_COUNT, sizeof(THREAD_DATA));
    ASSERT_IS_NOT_NULL(thread_data, "thread_data creation failed");

    for (i = 0; i < THREAD_COUNT; i++)
    {
        size_t j;

        thread_data[i].set = set;

        for (j = 0; j < INSERT_COUNT; j++)
        {
            thread_data[i].items[j] = malloc(sizeof(TEST_SET_ITEM));
            ASSERT_IS_NOT_NULL(thread_data[i].items[j], "Failed allocating item %zu for thread %zu", j, i);
        }
    }

    // act
    for (i = 0; i < THREAD_COUNT; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&threads[i], insert_and_remove_thread, &thread_data[i]));
    }

    // assert
    for (i = 0; i < THREAD_COUNT; i++)
    {
        int thread_result;
        (void)ThreadAPI_Join(threads[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result, "Thread %zu failed", i);
    }

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
    for (i = 0; i < THREAD_COUNT; i++)
    {
        size_t j;
        for (j = 0; j < INSERT_COUNT; j++)
        {
            free(thread_data[i].items[j]);
        }
    }

    free(thread_data);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
