// Copyright (c) Microsoft. All rights reserved.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdlib.h>
#endif

#include "testrunnerswitcher.h"
#include "c_pal/gballoc_hl.h"
#include "zrpc/mpsc_lock_free_queue.h"
#include "c_pal/threadapi.h"

#define THREAD_COUNT 4
#define INSERT_COUNT 1000000

typedef struct TEST_MPSC_QUEUE_ITEM_TAG
{
    MPSC_LOCK_FREE_QUEUE_ITEM item;
} TEST_MPSC_QUEUE_ITEM;

typedef struct THREAD_DATA_TAG
{
    MPSC_LOCK_FREE_QUEUE_HANDLE queue;
    TEST_MPSC_QUEUE_ITEM* items[INSERT_COUNT];
} THREAD_DATA;

static int enqueue_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    for (i = 0; i < INSERT_COUNT; i++)
    {
        result = mpsc_lock_free_queue_enqueue(thread_data->queue, &thread_data->items[i]->item);
        ASSERT_ARE_EQUAL(int, 0, result, "mpsc_lock_free_queue_enqueue failed");
    }

    return result;
}

static int dequeue_thread(void* arg)
{
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = (MPSC_LOCK_FREE_QUEUE_HANDLE)arg;
    size_t dequeued_count = 0;

    while (dequeued_count < INSERT_COUNT * THREAD_COUNT)
    {
        MPSC_LOCK_FREE_QUEUE_ITEM* dequeued = mpsc_lock_free_queue_dequeue(queue);
        if (dequeued != NULL)
        {
            dequeued_count++;
        }
    }

    return 0;
}

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

BEGIN_TEST_SUITE(mpsc_lock_free_queue_inttests)

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

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_014: [ Concurrent mpsc_lock_free_queue_enqueue from multiple threads shall be safe. ]*/
TEST_FUNCTION(multiple_threads_adding_and_one_consuming_succeeds)
{
    size_t i;
    MPSC_LOCK_FREE_QUEUE_HANDLE queue;
    THREAD_HANDLE threads[THREAD_COUNT];
    THREAD_HANDLE dequeue_thread_handle;
    THREAD_DATA* thread_data;
    int thread_result;

    // arrange
    queue = mpsc_lock_free_queue_create();
    ASSERT_IS_NOT_NULL(queue, "mpsc_lock_free_queue_create failed");

    thread_data = (THREAD_DATA*)malloc(sizeof(THREAD_DATA) * THREAD_COUNT);
    ASSERT_IS_NOT_NULL(thread_data, "thread_data creation failed");

    for (i = 0; i < THREAD_COUNT; i++)
    {
        size_t j;

        thread_data[i].queue = queue;

        for (j = 0; j < INSERT_COUNT; j++)
        {
            thread_data[i].items[j] = (TEST_MPSC_QUEUE_ITEM*)malloc(sizeof(TEST_MPSC_QUEUE_ITEM));
            ASSERT_IS_NOT_NULL(thread_data[i].items[j], "Failed allocating item %zu for thread %zu", j, i);
        }
    }

    // start dequeue thread
    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&dequeue_thread_handle, dequeue_thread, queue));

    // act
    for (i = 0; i < THREAD_COUNT; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&threads[i], enqueue_thread, &thread_data[i]));
    }

    // assert
    for (i = 0; i < THREAD_COUNT; i++)
    {
        (void)ThreadAPI_Join(threads[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result, "Thread %zu failed", i);
    }

    (void)ThreadAPI_Join(dequeue_thread_handle, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result, "Dqueue thread failed");

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
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

END_TEST_SUITE(mpsc_lock_free_queue_inttests)
