// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "c_pal/thandle.h"
#include "c_pal/threadapi.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/interlocked.h"
#include "c_pal/interlocked_hl.h"
#include "c_pal/sync.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"
#include "clds/tcall_dispatcher_thread_notification_call.h"
#include "clds/thread_notifications_dispatcher.h"

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
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

TEST_FUNCTION(thread_notifications_dispatcher_init_deinit_works)
{
    // arrange

    // act
    int result = thread_notifications_dispatcher_init();

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);

    // clean
    thread_notifications_dispatcher_deinit();
}

static int thread_func_do_nothing(void* arg)
{
    (void)arg;
    return 0;
}

#define N_TARGETS 2 // number of call targets

#define EXPECT_NOTIFICATION_STATE_VALUES \
    EXPECT_NOTIFICATION_STATE_THREAD_ATTACH, \
    EXPECT_NOTIFICATION_STATE_THREAD_DETACH, \
    EXPECT_NOTIFICATION_STATE_DONE \

MU_DEFINE_ENUM(EXPECT_NOTIFICATION_STATE, EXPECT_NOTIFICATION_STATE_VALUES);
MU_DEFINE_ENUM_STRINGS(EXPECT_NOTIFICATION_STATE, EXPECT_NOTIFICATION_STATE_VALUES);

TEST_DEFINE_ENUM_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);

// This is a callback that is fired for all threads that are spun twice
// one time for attach, one time for detach
static void test_thread_notification_cb(void* context, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason)
{
    ASSERT_IS_NOT_NULL(context);
    int32_t* expect_notification_state_ptr = context;
    EXPECT_NOTIFICATION_STATE expect_notification_state = interlocked_add(expect_notification_state_ptr, 0);
    switch (expect_notification_state)
    {
    default:
        ASSERT_FAIL("Unexpected expect reason: %" PRI_MU_ENUM "", MU_ENUM_VALUE(EXPECT_NOTIFICATION_STATE, expect_notification_state));
        break;
    case EXPECT_NOTIFICATION_STATE_THREAD_ATTACH:
        ASSERT_ARE_EQUAL(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH, reason);
        (void)interlocked_exchange(expect_notification_state_ptr, EXPECT_NOTIFICATION_STATE_THREAD_DETACH);
        wake_by_address_all(expect_notification_state_ptr);
        break;
    case EXPECT_NOTIFICATION_STATE_THREAD_DETACH:
        ASSERT_ARE_EQUAL(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH, reason);
        (void)interlocked_exchange(expect_notification_state_ptr, EXPECT_NOTIFICATION_STATE_DONE);
        wake_by_address_all(expect_notification_state_ptr);
        break;
    }
}

TEST_FUNCTION(MU_C3(thread_notifications_dispatcher_creating_threads_dispatches_the_call_to_, N_TARGETS, _targets))
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_dispatcher_init());
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = thread_notifications_dispatcher_get_call_dispatcher();
    ASSERT_IS_NOT_NULL(call_dispatcher);
    TCALL_DISPATCHER_TARGET_HANDLE(THREAD_NOTIFICATION_CALL)* call_targets = malloc_2(N_TARGETS, sizeof(TCALL_DISPATCHER_TARGET_HANDLE(THREAD_NOTIFICATION_CALL)));
    ASSERT_IS_NOT_NULL(call_targets);
    volatile_atomic int32_t* expect_notification_states = malloc_2(N_TARGETS, sizeof(volatile_atomic int32_t));
    ASSERT_IS_NOT_NULL(expect_notification_states);

    for (uint32_t i = 0; i < N_TARGETS; i++)
    {
        (void)interlocked_exchange(&expect_notification_states[i], EXPECT_NOTIFICATION_STATE_THREAD_ATTACH);
        call_targets[i] = TCALL_DISPATCHER_REGISTER_TARGET(THREAD_NOTIFICATION_CALL)(call_dispatcher, test_thread_notification_cb, (void*)&expect_notification_states[i]);
    }

    // act
    THREAD_HANDLE thread;
    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&thread, thread_func_do_nothing, NULL));

    // assert
    // wait for notifications to happen
    for (uint32_t i = 0; i < N_TARGETS; i++)
    {
        ASSERT_ARE_EQUAL(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_OK, InterlockedHL_WaitForValue((volatile_atomic int32_t*)&expect_notification_states[i], EXPECT_NOTIFICATION_STATE_DONE, UINT32_MAX));
        TCALL_DISPATCHER_UNREGISTER_TARGET(THREAD_NOTIFICATION_CALL)(call_dispatcher, call_targets[i]);
    }

    // clean
    int dont_care;
    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(thread, &dont_care));
    TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&call_dispatcher, NULL);
    thread_notifications_dispatcher_deinit();
    free(call_targets);
    free((void*)expect_notification_states);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
