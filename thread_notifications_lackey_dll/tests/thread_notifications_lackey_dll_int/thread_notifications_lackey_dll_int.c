// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "c_pal/interlocked.h"
#include "c_pal/threadapi.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

void test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason)
{
    (void)reason;
}

TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
}

TEST_FUNCTION_INITIALIZE(method_init)
{
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

/* thread_notifications_lackey_dll_init_callback */

TEST_FUNCTION(thread_notifications_lackey_dll_init_callback_works)
{
    // arrange
    
    // act
    // assert
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    
    // clean
    thread_notifications_lackey_dll_deinit_callback();
}

#define N_THREADS 50
#define MAX_SLEEP_TIME 3000 // ms

static int test_thread_func(void* arg)
{
    (void)arg;

    uint32_t sleep_time = rand() * MAX_SLEEP_TIME / RAND_MAX;

    ThreadAPI_Sleep(sleep_time);

    return 0;
}

typedef struct THREAD_NOTIFICATION_STRUCT_TAG
{
    uint32_t tid;
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason;
} THREAD_NOTIFICATION_STRUCT;

static THREAD_HANDLE thread_handles[N_THREADS];
static volatile_atomic int32_t notifications_count;
static THREAD_NOTIFICATION_STRUCT thread_notifications[N_THREADS * 2];

void test_thread_notifications_callback_track_threads(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason)
{
    int32_t index = interlocked_increment(&notifications_count) - 1;
    thread_notifications[index].reason = reason;
    thread_notifications[index].tid = ThreadAPI_GetCurrentThreadId();
}

TEST_FUNCTION(MU_C3(thread_notifications_lackey_dll_attach_and_detach_of_, N_THREADS, works))
{
    // arrange
    (void)interlocked_exchange(&notifications_count, 0);
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_track_threads));

    for (uint32_t i = 0; i < N_THREADS; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&thread_handles[i], test_thread_func, NULL));
    }

    // act
    for (uint32_t i = 0; i < N_THREADS; i++)
    {
        int dont_care;
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(thread_handles[i], &dont_care));
    }

    // assert
    uint32_t attach_count = 0;
    uint32_t detach_count = 0;
    ASSERT_ARE_EQUAL(int32_t, N_THREADS * 2, interlocked_add(&notifications_count, 0));
    for (uint32_t i = 0; i < N_THREADS * 2; i++)
    {
        switch (thread_notifications[i].reason)
        {
        default:
            ASSERT_FAIL("Wrong reason: %" PRI_MU_ENUM "", MU_ENUM_VALUE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notifications[i].reason));
            break;

        case THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH:
            attach_count++;
            break;
        case THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH:
        {
            uint32_t j;

            // find the attach
            for (j = 0; j < i; j++)
            {
                if (
                    (thread_notifications[j].reason == THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH) &&
                    (thread_notifications[i].tid == thread_notifications[j].tid)
                    )
                {
                    break;
                }
            }

            if (j == i)
            {
                ASSERT_FAIL("Could not find attach for TID: %" PRIu32 "", thread_notifications[i].tid);
            }
            else
            {
                // clear the attach so that if we have the same TID show up again we match properly the attach and detach notifications
                thread_notifications[j].tid = 0;
            }

            detach_count++;
            break;
        }
        }
    }

    ASSERT_ARE_EQUAL(uint32_t, detach_count, attach_count);

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
